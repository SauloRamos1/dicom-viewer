#include "Windowing.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmimage/diregist.h> // links the color image classes (RGB, YBR, palette)
#include <dcmtk/dcmimgle/dcmimage.h>

#include "DicomFileState.h"
#include "Guarded.h"

namespace dv {

std::uint8_t applyWindow(double value, Voi voi) noexcept
{
    const double width = std::max(voi.width, 1.0);
    const double mid = voi.center - 0.5;
    const double half = (width - 1.0) / 2.0;
    if (value <= mid - half)
        return 0;
    if (value > mid + half)
        return 255;
    return static_cast<std::uint8_t>(std::lround(((value - mid) / (width - 1.0) + 0.5) * 255.0));
}

std::expected<Voi, DicomError> presetVoi(WindowPreset preset, const DicomFile& file, int frame)
{
    switch (preset) {
    case WindowPreset::CtBrain: return kCtBrain;
    case WindowPreset::CtLung: return kCtLung;
    case WindowPreset::CtBone: return kCtBone;
    case WindowPreset::CtAbdomen: return kCtAbdomen;
    case WindowPreset::FromFile:
        if (file.info().defaultVoi)
            return *file.info().defaultVoi;
        [[fallthrough]];
    case WindowPreset::MinMax: break;
    }

    const auto values = file.rawModalityValues(frame);
    if (!values)
        return std::unexpected(values.error());
    const auto [lo, hi] = std::ranges::minmax(*values);
    return Voi{(static_cast<double>(hi) + lo + 1.0) / 2.0, static_cast<double>(hi) - lo + 1.0};
}

namespace {

std::expected<RenderedImage, DicomError> decode(const DicomFile& file, int frame, const RenderParams& params)
{
    const ImageInfo& info = file.info();
    if (auto ok = checkFrame(info, frame); !ok)
        return std::unexpected(std::move(ok.error()));

    const std::scoped_lock lock(file.state().mutex);
    DcmDataset& dataset = file.state().dataset();
    DicomImage image(&dataset, dataset.getCurrentXfer(), 0, static_cast<unsigned long>(frame), 1);
    if (image.getStatus() != EIS_Normal) {
        const ErrorCode code = info.photometric == Photometric::Other ? ErrorCode::UnsupportedPhotometric
                                                                      : ErrorCode::CorruptPixelData;
        return std::unexpected(makeError(code, DicomImage::getString(image.getStatus())));
    }

    const bool monochrome = image.isMonochrome() != 0;
    if (monochrome) // the standard needs width >= 1; DCMTK rejects anything narrower
        image.setWindow(params.voi.center, std::max(params.voi.width, 1.0));

    const auto* pixels = static_cast<const std::uint8_t*>(image.getOutputData(8));
    const std::size_t size = image.getOutputDataSize(8);
    if (pixels == nullptr || size == 0)
        return std::unexpected(makeError(ErrorCode::DecodeFailed, DicomImage::getString(image.getStatus())));

    RenderedImage out;
    out.width = static_cast<int>(image.getWidth());
    out.height = static_cast<int>(image.getHeight());
    out.channels = monochrome ? 1 : 3;
    out.data.assign(pixels, pixels + size);
    return out;
}

// Invert (always) then colormap (monochrome only), in place.
void stylize(RenderedImage& image, const RenderParams& params)
{
    if (params.invert) {
        for (std::uint8_t& v : image.data)
            v = static_cast<std::uint8_t>(255 - v);
    }
    if (image.channels == 1 && params.colormap != Colormap::Gray) {
        const ColorLut& lut = colormapLut(params.colormap);
        std::vector<std::uint8_t> colored(image.data.size() * 3);
        for (std::size_t i = 0; i < image.data.size(); ++i)
            std::memcpy(&colored[i * 3], lut[image.data[i]].data(), 3);
        image.data = std::move(colored);
        image.channels = 3;
    }
}

} // namespace

std::expected<RenderedImage, DicomError> renderFrame(const DicomFile& file, int frame, const RenderParams& params)
{
    return guarded<RenderedImage>([&]() -> std::expected<RenderedImage, DicomError> {
        auto image = decode(file, frame, params);
        if (image)
            stylize(*image, params);
        return image;
    });
}

} // namespace dv
