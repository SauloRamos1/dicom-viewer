// SPDX-License-Identifier: GPL-3.0-only
#include "Heightmap.h"

#include <algorithm>
#include <cmath>

namespace dv {

namespace {

constexpr int kColourChannels = 3;

int cellCount(int pixels, int step)
{
    return std::max(2, (pixels + step - 1) / step);
}

// Pixel range [first, last) averaged by one grid cell; clamped so a side with
// fewer pixels than the 2 vertices it must have still reads a pixel.
struct Cell {
    int first;
    int last;
};
Cell cellOf(int index, int step, int pixels)
{
    const int first = std::min(index * step, pixels - 1);
    return {first, std::min(pixels, first + step)};
}

} // namespace

HeightmapMesh buildHeightmap(int width, int height, std::span<const std::uint8_t> levels,
                             std::span<const std::uint8_t> rgb, int maxVertices)
{
    const auto pixelCount = static_cast<std::size_t>(std::max(width, 0)) * static_cast<std::size_t>(std::max(height, 0));
    if (width <= 0 || height <= 0 || maxVertices < 2 || levels.size() != pixelCount
        || (!rgb.empty() && rgb.size() != pixelCount * kColourChannels))
        return {};

    HeightmapMesh mesh;
    const int longest = std::max(width, height);
    mesh.step = std::max(1, (longest + maxVertices - 1) / maxVertices);
    mesh.columns = cellCount(width, mesh.step);
    mesh.rows = cellCount(height, mesh.step);

    const float extentX = kHeightmapExtent * static_cast<float>(width) / static_cast<float>(longest);
    const float extentZ = kHeightmapExtent * static_cast<float>(height) / static_cast<float>(longest);
    mesh.boundsMin = {-extentX / 2, -kHeightmapExtent / 2, -extentZ / 2};
    mesh.boundsMax = {extentX / 2, kHeightmapExtent / 2, extentZ / 2};

    const auto vertexCount = static_cast<std::size_t>(mesh.columns) * static_cast<std::size_t>(mesh.rows);
    std::vector<float> heights(vertexCount);
    mesh.vertices.resize(vertexCount * kHeightmapVertexFloats);

    for (int row = 0; row < mesh.rows; ++row) {
        const Cell rowCell = cellOf(row, mesh.step, height);
        for (int column = 0; column < mesh.columns; ++column) {
            const Cell columnCell = cellOf(column, mesh.step, width);
            float level = 0;
            std::array<float, kColourChannels> colour{};
            for (int y = rowCell.first; y < rowCell.last; ++y) {
                for (int x = columnCell.first; x < columnCell.last; ++x) {
                    const auto pixel = static_cast<std::size_t>(y) * width + x;
                    level += levels[pixel];
                    for (int c = 0; c < kColourChannels; ++c)
                        colour[c] += rgb.empty() ? levels[pixel] : rgb[pixel * kColourChannels + c];
                }
            }
            const float count = static_cast<float>((rowCell.last - rowCell.first) * (columnCell.last - columnCell.first));
            const auto index = static_cast<std::size_t>(row) * mesh.columns + column;
            heights[index] = level / count / 255.0F;

            float* v = mesh.vertices.data() + index * kHeightmapVertexFloats;
            v[0] = (static_cast<float>(column) / static_cast<float>(mesh.columns - 1) - 0.5F) * extentX;
            v[1] = (heights[index] - 0.5F) * kHeightmapExtent;
            v[2] = (static_cast<float>(row) / static_cast<float>(mesh.rows - 1) - 0.5F) * extentZ;
            v[6] = colour[0] / count / 255.0F;
            v[7] = colour[1] / count / 255.0F;
            v[8] = colour[2] / count / 255.0F;
            v[9] = 1.0F;
        }
    }

    // Normals from central differences of the height field (one-sided at the edges).
    const float cellX = extentX / static_cast<float>(mesh.columns - 1);
    const float cellZ = extentZ / static_cast<float>(mesh.rows - 1);
    const auto heightAt = [&](int column, int row) {
        return heights[static_cast<std::size_t>(row) * mesh.columns + column] * kHeightmapExtent;
    };
    for (int row = 0; row < mesh.rows; ++row) {
        const int up = std::max(row - 1, 0);
        const int down = std::min(row + 1, mesh.rows - 1);
        for (int column = 0; column < mesh.columns; ++column) {
            const int left = std::max(column - 1, 0);
            const int right = std::min(column + 1, mesh.columns - 1);
            const float slopeX = (heightAt(right, row) - heightAt(left, row)) / (static_cast<float>(right - left) * cellX);
            const float slopeZ = (heightAt(column, down) - heightAt(column, up)) / (static_cast<float>(down - up) * cellZ);
            const float length = std::sqrt(slopeX * slopeX + 1.0F + slopeZ * slopeZ);
            float* v = mesh.vertices.data() + (static_cast<std::size_t>(row) * mesh.columns + column) * kHeightmapVertexFloats;
            v[3] = -slopeX / length;
            v[4] = 1.0F / length;
            v[5] = -slopeZ / length;
        }
    }

    mesh.indices.reserve(static_cast<std::size_t>(mesh.columns - 1) * static_cast<std::size_t>(mesh.rows - 1) * 6);
    for (int row = 0; row + 1 < mesh.rows; ++row) {
        for (int column = 0; column + 1 < mesh.columns; ++column) {
            const auto topLeft = static_cast<std::uint32_t>(row * mesh.columns + column);
            const auto topRight = topLeft + 1;
            const auto bottomLeft = topLeft + static_cast<std::uint32_t>(mesh.columns);
            const auto bottomRight = bottomLeft + 1;
            mesh.indices.insert(mesh.indices.end(), {topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight});
        }
    }
    return mesh;
}

} // namespace dv
