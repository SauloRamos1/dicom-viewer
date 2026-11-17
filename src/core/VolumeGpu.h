// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Volume.h"

namespace dv {

// Volume upload for the 3D view. The ray-march shader samples one R16 (unorm
// 16-bit) 3D texture: the modality values are mapped linearly from
// [valueMin, valueMax] to 0..65535, so the shader rebuilds
//   value = valueMin + sample * (valueMax - valueMin)
// and then windows it like the 2D view. 16 bits keep a 12-bit CT range lossless
// (8 bits would leave a narrow window with a handful of levels); R16 unorm is a
// filterable format on Metal, D3D, Vulkan and GL, unlike integer textures.
//
// The upload is capped at kVolumeBudgetBytes (and kMaxTextureDimension per side)
// by averaging whole blocks of f x f x f voxels, f the smallest integer that fits.
inline constexpr std::size_t kVolumeBudgetBytes = std::size_t{128} << 20;
inline constexpr int kMaxTextureDimension = 2048;

struct PackedVolume {
    int width{};
    int height{};
    int depth{};
    std::array<double, 3> spacing{1.0, 1.0, 1.0}; // millimetres per texel
    double valueMin{};
    double valueMax{1.0};
    int downsample{1}; // factor applied to the source volume
    std::vector<std::uint16_t> voxels; // [(z * height + y) * width + x]
};

// Smallest f >= 1 such that ceil(w/f) * ceil(h/f) * ceil(d/f) * bytesPerVoxel
// <= budgetBytes and no side exceeds maxDimension.
[[nodiscard]] int downsampleFactor(int width, int height, int depth, std::size_t bytesPerVoxel,
                                   std::size_t budgetBytes = kVolumeBudgetBytes,
                                   int maxDimension = kMaxTextureDimension);

// Averages blocks of factor^3 voxels (edge blocks hold fewer); sides become
// ceil(n / factor) and the spacing grows by the factor. Factor 1 is a copy.
[[nodiscard]] Volume downsample(const Volume& volume, int factor);

// Downsamples as needed for `budgetBytes`, then maps the values to 16 bits.
[[nodiscard]] PackedVolume packVolume(const Volume& volume, std::size_t budgetBytes = kVolumeBudgetBytes);

} // namespace dv
