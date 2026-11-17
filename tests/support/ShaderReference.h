// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// float32 mirror of src/app/shaders/windowing.frag, the reference the packed
// texture and the CPU renderer are compared against. Keep the two in step.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "GpuTexture.h"
#include "Windowing.h"

namespace dv::test {

// What the shader does to one packed texel, given the uniforms.
struct ShaderState {
    Voi voi;
    bool invert{false};
    Colormap colormap{Colormap::Gray};
};

// u = round(R*255)*256 + round(G*255); R and G arrive as normalised floats.
inline float shaderUnpack(std::uint8_t r, std::uint8_t g)
{
    const float rn = static_cast<float>(r) / 255.0F;
    const float gn = static_cast<float>(g) / 255.0F;
    return std::floor(rn * 255.0F + 0.5F) * 256.0F + std::floor(gn * 255.0F + 0.5F);
}

inline float shaderValue(std::uint8_t r, std::uint8_t g, const GpuParams& params)
{
    return (shaderUnpack(r, g) + static_cast<float>(params.storedMin)) * static_cast<float>(params.slope)
        + static_cast<float>(params.intercept);
}

// Gray level (0..255) after window, polarity and invert. Like DCMTK's linear VOI
// (dimoopxt.h, "VOI LINEAR #8") the scaled value is truncated, not rounded, and
// MONOCHROME1 truncates the already inverted ramp, so the shader reproduces the
// CPU renderer instead of applyWindow (which rounds; the two differ by one level).
inline int shaderGray(float value, const GpuParams& params, const ShaderState& state)
{
    const float width = std::max(static_cast<float>(state.voi.width), 1.0F);
    const float mid = static_cast<float>(state.voi.center) - 0.5F;
    const float span = width - 1.0F;
    float t = span > 0.0F ? (value - mid) / span + 0.5F : (value > mid ? 1.0F : 0.0F);
    t = std::clamp(t, 0.0F, 1.0F);
    int gray = static_cast<int>(std::floor((params.monochrome1 ? 1.0F - t : t) * 255.0F));
    if (state.invert)
        gray = 255 - gray;
    return gray;
}

inline std::array<std::uint8_t, 3> shaderPixel(std::uint8_t r, std::uint8_t g, const GpuParams& params,
                                               const ShaderState& state)
{
    return colormapLut(state.colormap)[static_cast<std::size_t>(shaderGray(shaderValue(r, g, params), params, state))];
}

// Whole frame through the shader math: interleaved RGB (3 bytes per pixel).
inline std::vector<std::uint8_t> shaderRender(const PackedFrame& frame, const GpuParams& params,
                                              const ShaderState& state)
{
    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 3);
    for (std::size_t i = 0; i + 3 < frame.rgba.size(); i += 4) {
        const auto rgb = shaderPixel(frame.rgba[i], frame.rgba[i + 1], params, state);
        out.insert(out.end(), rgb.begin(), rgb.end());
    }
    return out;
}

} // namespace dv::test
