#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include "DicomError.h"
#include "DicomFile.h"

namespace dv {

// Raw-frame upload for the GPU render path. Qt Quick textures are 8 bits per
// channel, so a stored sample of up to 16 bits travels in two channels of an
// RGBA8 texture:
//
//   R = high byte, G = low byte, B = 0, A = 255
//
// of u = stored - storedMin, with storedMin = -2^(bitsStored-1) for signed
// pixel data and 0 otherwise (u is always in 0 .. 2^bitsStored - 1). The shader
// rebuilds u = round(R*255)*256 + round(G*255) and then follows the CPU path:
//
//   value = (u + storedMin) * slope + intercept      (modality value)
//   t     = clamp(((value - (c - 0.5)) / (w - 1) + 0.5), 0, 1)   (PS3.3 C.11.2.1.2.1)
//   byte  = floor(255 * t), or floor(255 * (1 - t)) for MONOCHROME1
//   byte  = 255 - byte  for the Invert option
// The truncation (rather than applyWindow's rounding) is what DCMTK does in the
// CPU path, so both backends show the same gray levels.
//   rgb   = colormap LUT[byte]
struct GpuParams {
    double storedMin{};
    double slope{1.0};
    double intercept{};
    bool monochrome1{false};
};

// True when the shader can render `file`: a single-sample monochrome image of
// 8 or 16 allocated bits without a Modality LUT Sequence (which only the CPU
// path applies). Anything else (colour, 32-bit, ...) uses the CPU path.
[[nodiscard]] bool gpuSupported(const DicomFile& file);

// Shader constants for `info` (meaningful when the file is gpuSupported).
[[nodiscard]] GpuParams gpuParams(const ImageInfo& info) noexcept;

struct PackedFrame {
    int width{};
    int height{};
    std::vector<std::uint8_t> rgba; // width * height * 4, row-major, row 0 = top
};

// Packs one frame as described above. UnsupportedPhotometric when
// !gpuSupported(file); FrameOutOfRange for a bad `frame`.
[[nodiscard]] std::expected<PackedFrame, DicomError> packFrame(const DicomFile& file, int frame);

} // namespace dv
