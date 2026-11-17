#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <vector>

#include "DicomError.h"
#include "DicomFile.h"
#include "Voi.h"

namespace dv {

enum class Colormap { Gray, Bone, Hot, Jet };

enum class WindowPreset { FromFile, MinMax, CtBrain, CtLung, CtBone, CtAbdomen };

struct RenderParams {
    Voi voi;
    bool invert{false};
    Colormap colormap{Colormap::Gray};
};

// 8-bit output image, row-major, interleaved: channels is 1 (gray) or 3 (RGB).
struct RenderedImage {
    int width{};
    int height{};
    int channels{1};
    std::vector<std::uint8_t> data;
};

using ColorLut = std::array<std::array<std::uint8_t, 3>, 256>;

inline constexpr Voi kCtBrain{40, 80};
inline constexpr Voi kCtLung{-600, 1500};
inline constexpr Voi kCtBone{400, 1800};
inline constexpr Voi kCtAbdomen{40, 400};

// DICOM linear VOI function (PS3.3 C.11.2.1.2.1) mapped to 0..255:
//   x <= c - 0.5 - (w-1)/2  ->  0
//   x >  c - 0.5 + (w-1)/2  ->  255
//   otherwise               ->  ((x - (c - 0.5)) / (w-1) + 0.5) * 255
// The standard requires w >= 1; narrower widths are treated as 1 (a binary
// threshold at the centre). Result is rounded to nearest.
[[nodiscard]] std::uint8_t applyWindow(double value, Voi voi) noexcept;

// Window for `preset`. FromFile uses the file's first window, falling back to
// MinMax; MinMax spans the modality values of `frame` (monochrome only).
[[nodiscard]] std::expected<Voi, DicomError> presetVoi(WindowPreset preset, const DicomFile& file,
                                                       int frame = 0);

// 256-entry color table for a colormap (Gray is the identity).
[[nodiscard]] const ColorLut& colormapLut(Colormap colormap);

// Renders one frame to 8 bits using DCMTK for the modality LUT, MONOCHROME1
// handling and RGB/YBR/palette conversion. For monochrome images the VOI
// window, optional invert and colormap are applied (a colormap makes the
// output 3-channel). Color images ignore voi/colormap; invert still applies.
// Orientation: pixels are in file order (row 0 = top), already upright per DICOM.
[[nodiscard]] std::expected<RenderedImage, DicomError> renderFrame(const DicomFile& file, int frame,
                                                                    const RenderParams& params);

} // namespace dv
