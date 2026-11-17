// DicomFile pixel access: modality values of monochrome frames.

#include <cstdint>
#include <mutex>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "DicomFile.h"
#include "DicomFileState.h"
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

    float at(std::size_t index) const
    {
        const std::uint32_t raw = data8_ != nullptr ? data8_[index] : data16_[index];
        std::int32_t stored = static_cast<std::int32_t>(raw & mask_);
        if (signed_ && (stored & signBit_) != 0)
            stored -= static_cast<std::int32_t>(mask_) + 1;
        return static_cast<float>(stored * slope_ + intercept_);
    }

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

} // namespace dv
