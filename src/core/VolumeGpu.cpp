// SPDX-License-Identifier: GPL-3.0-only
#include "VolumeGpu.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dv {

namespace {

int ceilDiv(int n, int f)
{
    return (n + f - 1) / f;
}

bool fits(int width, int height, int depth, int factor, std::size_t bytesPerVoxel, std::size_t budgetBytes,
          int maxDimension)
{
    const int w = ceilDiv(width, factor);
    const int h = ceilDiv(height, factor);
    const int d = ceilDiv(depth, factor);
    if (std::max({w, h, d}) > maxDimension)
        return false;
    return static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(d) * bytesPerVoxel
        <= budgetBytes;
}

} // namespace

int downsampleFactor(int width, int height, int depth, std::size_t bytesPerVoxel, std::size_t budgetBytes,
                     int maxDimension)
{
    int factor = 1;
    const int largest = std::max({width, height, depth, 1});
    while (factor < largest && !fits(width, height, depth, factor, bytesPerVoxel, budgetBytes, maxDimension))
        ++factor;
    return factor;
}

Volume downsample(const Volume& volume, int factor)
{
    if (factor <= 1)
        return volume;
    Volume out{.width = ceilDiv(volume.width, factor),
               .height = ceilDiv(volume.height, factor),
               .depth = ceilDiv(volume.depth, factor),
               .spacing = {volume.spacing[0] * factor, volume.spacing[1] * factor, volume.spacing[2] * factor}};
    out.voxels.resize(static_cast<std::size_t>(out.width) * out.height * out.depth);

    const auto index = [&](int x, int y, int z) {
        return (static_cast<std::size_t>(z) * volume.height + y) * volume.width + x;
    };
    std::size_t next = 0;
    for (int z = 0; z < out.depth; ++z) {
        const int z1 = std::min(volume.depth, (z + 1) * factor);
        for (int y = 0; y < out.height; ++y) {
            const int y1 = std::min(volume.height, (y + 1) * factor);
            for (int x = 0; x < out.width; ++x) {
                const int x1 = std::min(volume.width, (x + 1) * factor);
                double sum = 0;
                for (int sz = z * factor; sz < z1; ++sz)
                    for (int sy = y * factor; sy < y1; ++sy)
                        for (int sx = x * factor; sx < x1; ++sx)
                            sum += volume.voxels[index(sx, sy, sz)];
                const double count = static_cast<double>(z1 - z * factor) * (y1 - y * factor) * (x1 - x * factor);
                out.voxels[next++] = static_cast<float>(sum / count);
            }
        }
    }
    return out;
}

PackedVolume packVolume(const Volume& volume, std::size_t budgetBytes)
{
    const int factor = downsampleFactor(volume.width, volume.height, volume.depth, sizeof(std::uint16_t), budgetBytes);
    const Volume reduced = downsample(volume, factor);

    PackedVolume packed{.width = reduced.width,
                        .height = reduced.height,
                        .depth = reduced.depth,
                        .spacing = reduced.spacing,
                        .downsample = factor};
    const auto [lowest, highest] = std::ranges::minmax_element(reduced.voxels);
    if (lowest == reduced.voxels.end()) // empty volume
        return packed;
    packed.valueMin = *lowest;
    packed.valueMax = *highest > *lowest ? static_cast<double>(*highest) : packed.valueMin + 1.0;

    const double scale = static_cast<double>(std::numeric_limits<std::uint16_t>::max()) / (packed.valueMax - packed.valueMin);
    packed.voxels.resize(reduced.voxels.size());
    std::ranges::transform(reduced.voxels, packed.voxels.begin(), [&](float v) {
        return static_cast<std::uint16_t>(std::lround((v - packed.valueMin) * scale));
    });
    return packed;
}

} // namespace dv
