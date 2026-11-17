// SPDX-License-Identifier: GPL-3.0-only
// DicomFile pixel access: modality values of monochrome frames.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include "DicomFile.h"
#include "DicomFileState.h"
#include "GpuTexture.h"
#include "Guarded.h"

namespace dv {

namespace {

// Modality LUT Sequence (0028,3000), first item: stored values from `first` on index `data`, the
// first and last entries repeating outside the table. `data` points into the dataset.
struct ModalityLut {
    std::int32_t first{};
    const Uint16* data{};
    std::size_t size{};
};

// Entry `pos` of the LUT Descriptor (US, or SS for signed data): entries, first mapped value, bits.
std::optional<std::int32_t> descriptorValue(DcmItem& item, unsigned long pos, bool isSigned)
{
    Uint16 unsignedValue = 0;
    if (item.findAndGetUint16(DCM_LUTDescriptor, unsignedValue, pos).good())
        return isSigned && pos == 1 ? static_cast<std::int16_t>(unsignedValue) : unsignedValue;
    Sint16 signedValue = 0;
    if (item.findAndGetSint16(DCM_LUTDescriptor, signedValue, pos).good())
        return signedValue;
    return std::nullopt;
}

// The Modality LUT of `ds`, nullopt when there is none; an error when it is unusable.
std::expected<std::optional<ModalityLut>, DicomError> readModalityLut(DcmDataset& ds, bool isSigned)
{
    DcmSequenceOfItems* sequence = nullptr;
    if (ds.findAndGetSequence(DCM_ModalityLUTSequence, sequence).bad() || sequence == nullptr || sequence->card() == 0)
        return std::optional<ModalityLut>{};
    DcmItem* item = sequence->getItem(0);
    const auto entries = item != nullptr ? descriptorValue(*item, 0, isSigned) : std::nullopt;
    const auto first = item != nullptr ? descriptorValue(*item, 1, isSigned) : std::nullopt;
    const Uint16* data = nullptr;
    unsigned long count = 0;
    // A descriptor of 0 entries means 65536.
    const std::size_t size = entries && (*entries & 0xFFFF) != 0 ? static_cast<std::size_t>(*entries & 0xFFFF) : 65536;
    if (!entries || !first || item->findAndGetUint16Array(DCM_LUTData, data, &count).bad() || data == nullptr
        || count < size)
        return std::unexpected(makeError(ErrorCode::CorruptPixelData, "Unusable Modality LUT", "(0028,3000)"));
    return ModalityLut{.first = *first, .data = data, .size = size};
}

// Read-only view of the (decoded, native byte order) pixel data of a
// monochrome image, converting stored samples to modality values.
class SampleReader {
public:
    static std::expected<SampleReader, DicomError> create(DcmDataset& ds, const ImageInfo& info)
    {
        if (!info.isMonochrome() || info.samplesPerPixel != 1)
            return std::unexpected(makeError(ErrorCode::UnsupportedPhotometric,
                                             "Modality values need a single-sample monochrome image"));
        // The shifts in the constructor need 1 <= BitsStored <= BitsAllocated <= 16.
        const auto layoutError = [&] {
            return std::unexpected(makeError(ErrorCode::CorruptPixelData,
                                             "Unsupported or unreadable pixel data layout (BitsAllocated "
                                                 + std::to_string(info.bitsAllocated) + ", BitsStored "
                                                 + std::to_string(info.bitsStored) + ")",
                                             "(7FE0,0010)"));
        };
        if ((info.bitsAllocated != 8 && info.bitsAllocated != 16) || info.bitsStored < 1
            || info.bitsStored > info.bitsAllocated)
            return layoutError();
        SampleReader reader(info);
        const OFCondition status = info.bitsAllocated == 8
            ? ds.findAndGetUint8Array(DCM_PixelData, reader.data8_)
            : ds.findAndGetUint16Array(DCM_PixelData, reader.data16_);
        if (status.bad() || (reader.data8_ == nullptr && reader.data16_ == nullptr))
            return layoutError();
        auto lut = readModalityLut(ds, reader.signed_);
        if (!lut)
            return std::unexpected(std::move(lut.error()));
        reader.lut_ = *lut;
        return reader;
    }

    // Stored sample (masked to BitsStored, sign-extended for signed data).
    std::int32_t storedAt(std::size_t index) const
    {
        const std::uint32_t raw = data8_ != nullptr ? data8_[index] : data16_[index];
        std::int32_t stored = static_cast<std::int32_t>(raw & mask_);
        if (signed_ && (stored & signBit_) != 0)
            stored -= static_cast<std::int32_t>(mask_) + 1;
        return stored;
    }

    // Modality value: through the Modality LUT when the file has one, else stored * slope + intercept.
    float at(std::size_t index) const
    {
        const std::int32_t stored = storedAt(index);
        if (lut_) {
            const std::int64_t entry = std::clamp<std::int64_t>(static_cast<std::int64_t>(stored) - lut_->first, 0,
                                                                 static_cast<std::int64_t>(lut_->size) - 1);
            return static_cast<float>(lut_->data[entry]);
        }
        return static_cast<float>(stored * slope_ + intercept_);
    }

private:
    explicit SampleReader(const ImageInfo& info)
        : mask_((1u << info.bitsStored) - 1u)
        , signBit_(static_cast<std::int32_t>(1u << (info.bitsStored - 1)))
        , signed_(info.pixelRepresentation == 1)
        , slope_(info.rescaleSlope)
        , intercept_(info.rescaleIntercept)
    {
    }

    std::optional<ModalityLut> lut_;
    const Uint8* data8_{};
    const Uint16* data16_{};
    std::uint32_t mask_;
    std::int32_t signBit_;
    bool signed_;
    double slope_;
    double intercept_;
};

std::size_t frameSize(const ImageInfo& info)
{
    return static_cast<std::size_t>(info.rows) * static_cast<std::size_t>(info.columns);
}

} // namespace

std::expected<std::vector<float>, DicomError> DicomFile::rawModalityValues(int frame) const
{
    return guarded<std::vector<float>>([&]() -> std::expected<std::vector<float>, DicomError> {
        if (auto ok = checkFrame(info_, frame); !ok)
            return std::unexpected(std::move(ok.error()));
        const std::scoped_lock lock(state_->mutex);
        auto reader = SampleReader::create(state_->dataset(), info_);
        if (!reader)
            return std::unexpected(std::move(reader.error()));
        const std::size_t size = frameSize(info_);
        const std::size_t offset = size * static_cast<std::size_t>(frame);
        std::vector<float> values(size);
        for (std::size_t i = 0; i < size; ++i)
            values[i] = reader->at(offset + i);
        return values;
    });
}

std::expected<float, DicomError> DicomFile::valueAt(int x, int y, int frame) const
{
    return guarded<float>([&]() -> std::expected<float, DicomError> {
        if (auto ok = checkFrame(info_, frame); !ok)
            return std::unexpected(std::move(ok.error()));
        if (x < 0 || y < 0 || x >= info_.columns || y >= info_.rows)
            return std::unexpected(makeError(ErrorCode::FrameOutOfRange,
                                             "pixel (" + std::to_string(x) + "," + std::to_string(y) + ")"));
        const std::scoped_lock lock(state_->mutex);
        auto reader = SampleReader::create(state_->dataset(), info_);
        if (!reader)
            return std::unexpected(std::move(reader.error()));
        const std::size_t index = frameSize(info_) * static_cast<std::size_t>(frame)
            + static_cast<std::size_t>(y) * static_cast<std::size_t>(info_.columns) + static_cast<std::size_t>(x);
        return reader->at(index);
    });
}

bool gpuSupported(const DicomFile& file)
{
    const ImageInfo& info = file.info();
    if (!info.isMonochrome() || info.samplesPerPixel != 1 || (info.bitsAllocated != 8 && info.bitsAllocated != 16)
        || info.bitsStored < 1 || info.bitsStored > 16)
        return false;
    const std::scoped_lock lock(file.state().mutex);
    DcmDataset& dataset = file.state().dataset();
    DcmSequenceOfItems* modalityLut = nullptr;
    if (dataset.findAndGetSequence(DCM_ModalityLUTSequence, modalityLut).good() && modalityLut != nullptr
        && modalityLut->card() > 0)
        return false;
    // The shader has the LINEAR function only; SIGMOID and LINEAR_EXACT differ on the CPU path.
    OFString voiFunction;
    return !(dataset.findAndGetOFString(DCM_VOILUTFunction, voiFunction).good() && !voiFunction.empty()
             && voiFunction != "LINEAR");
}

bool gpuSupported(std::span<const DicomFile> slices)
{
    if (slices.empty() || !std::ranges::all_of(slices, [](const DicomFile& slice) { return gpuSupported(slice); }))
        return false;
    const GpuParams first = gpuParams(slices.front().info());
    return std::ranges::all_of(slices, [&](const DicomFile& slice) { return gpuParams(slice.info()) == first; });
}

GpuParams gpuParams(const ImageInfo& info) noexcept
{
    return {.storedMin = info.pixelRepresentation == 1 ? -std::ldexp(1.0, info.bitsStored - 1) : 0.0,
            .slope = info.rescaleSlope,
            .intercept = info.rescaleIntercept,
            .monochrome1 = info.photometric == Photometric::Monochrome1};
}

std::expected<PackedFrame, DicomError> packFrame(const DicomFile& file, int frame)
{
    return guarded<PackedFrame>([&]() -> std::expected<PackedFrame, DicomError> {
        const ImageInfo& info = file.info();
        if (auto ok = checkFrame(info, frame); !ok)
            return std::unexpected(std::move(ok.error()));
        if (!gpuSupported(file))
            return std::unexpected(makeError(ErrorCode::UnsupportedPhotometric,
                                             "Frame cannot be packed for the GPU (not 8/16-bit monochrome)"));
        const std::scoped_lock lock(file.state().mutex);
        auto reader = SampleReader::create(file.state().dataset(), info);
        if (!reader)
            return std::unexpected(std::move(reader.error()));

        const auto storedMin = static_cast<std::int32_t>(gpuParams(info).storedMin);
        const std::size_t size = frameSize(info);
        const std::size_t offset = size * static_cast<std::size_t>(frame);
        PackedFrame out{.width = info.columns, .height = info.rows, .rgba = std::vector<std::uint8_t>(size * 4)};
        std::uint8_t* dst = out.rgba.data();
        for (std::size_t i = 0; i < size; ++i, dst += 4) {
            const auto u = static_cast<std::uint32_t>(reader->storedAt(offset + i) - storedMin);
            dst[0] = static_cast<std::uint8_t>(u >> 8);
            dst[1] = static_cast<std::uint8_t>(u & 0xFFU);
            dst[2] = 0;
            dst[3] = 255;
        }
        return out;
    });
}

} // namespace dv
