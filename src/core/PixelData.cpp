// SPDX-License-Identifier: GPL-3.0-only
// DicomFile pixel access: modality values of monochrome frames.

#include <cmath>
#include <cstdint>
#include <mutex>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include "DicomFile.h"
#include "DicomFileState.h"
#include "GpuTexture.h"
#include "Guarded.h"

namespace dv {

namespace {

// Read-only view of the (decoded, native byte order) pixel data of a
// monochrome image, converting stored samples to modality values.
class SampleReader {
public:
    static std::expected<SampleReader, DicomError> create(DcmDataset& ds, const ImageInfo& info)
    {
        if (!info.isMonochrome() || info.samplesPerPixel != 1)
            return std::unexpected(makeError(ErrorCode::UnsupportedPhotometric,
                                             "Modality values need a single-sample monochrome image"));
        SampleReader reader(info);
        const OFCondition status = info.bitsAllocated == 8
            ? ds.findAndGetUint8Array(DCM_PixelData, reader.data8_)
            : ds.findAndGetUint16Array(DCM_PixelData, reader.data16_);
        if ((info.bitsAllocated != 8 && info.bitsAllocated != 16) || status.bad()
            || (reader.data8_ == nullptr && reader.data16_ == nullptr)) {
            return std::unexpected(makeError(ErrorCode::CorruptPixelData,
                                             "Unsupported or unreadable pixel data layout (BitsAllocated "
                                                 + std::to_string(info.bitsAllocated) + ")",
                                             "(7FE0,0010)"));
        }
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

    float at(std::size_t index) const { return static_cast<float>(storedAt(index) * slope_ + intercept_); }

private:
    explicit SampleReader(const ImageInfo& info)
        : mask_(info.bitsStored >= 32 ? 0xFFFFFFFFu : (1u << info.bitsStored) - 1u)
        , signBit_(static_cast<std::int32_t>(1u << (info.bitsStored - 1)))
        , signed_(info.pixelRepresentation == 1)
        , slope_(info.rescaleSlope)
        , intercept_(info.rescaleIntercept)
    {
    }

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
    DcmSequenceOfItems* modalityLut = nullptr;
    return !(file.state().dataset().findAndGetSequence(DCM_ModalityLUTSequence, modalityLut).good()
             && modalityLut != nullptr && modalityLut->card() > 0);
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
