// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace dv {

// Triangle mesh of an image seen as a relief: the height of a vertex is the
// windowed intensity of the pixels around it. Pure geometry, no Qt.
//
// Layout of `vertices`, kHeightmapVertexFloats floats per vertex, row-major over
// the (columns x rows) grid with grid row 0 = image row 0:
//   position (x, y, z)  x to the right, y up (height), z towards the viewer
//   normal   (nx, ny, nz)  unit length, from the height field at scale 1
//   colour   (r, g, b, a)  0..1, a = 1
// The footprint is centred on the origin and its longer side is
// kHeightmapExtent long; heights span -kHeightmapExtent/2 (level 0) to
// +kHeightmapExtent/2 (level 255), so the depth scale of a view is simply the
// model's y scale (the normal matrix keeps the lighting right).
inline constexpr int kHeightmapVertexFloats = 10;
inline constexpr float kHeightmapExtent = 100.0F;
inline constexpr int kHeightmapMaxVertices = 512; // per side

struct HeightmapMesh {
    int columns{};  // vertex grid size
    int rows{};
    int step{1};    // source pixels per grid cell, per side
    std::array<float, 3> boundsMin{};
    std::array<float, 3> boundsMax{};
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices; // counter-clockwise from above (+y)
};

// `levels`: width * height windowed gray levels (0..255), row 0 on top. `rgb`:
// width * height * 3 colours, or empty to colour by gray level. The grid is
// decimated by a whole step (the smallest that fits `maxVertices` per side) and
// every cell averages its pixels; each side has at least 2 vertices so even a
// 1 x N image gives triangles. Wrong buffer sizes give an empty mesh.
[[nodiscard]] HeightmapMesh buildHeightmap(int width, int height, std::span<const std::uint8_t> levels,
                                           std::span<const std::uint8_t> rgb = {},
                                           int maxVertices = kHeightmapMaxVertices);

} // namespace dv
