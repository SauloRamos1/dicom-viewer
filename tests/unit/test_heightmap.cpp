#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

#include "Heightmap.h"

using Catch::Approx;
using dv::kHeightmapExtent;
using dv::kHeightmapVertexFloats;

namespace {

struct Vertex {
    float x, y, z, nx, ny, nz, r, g, b, a;
};

Vertex vertexAt(const dv::HeightmapMesh& mesh, int column, int row)
{
    const auto* v = mesh.vertices.data() + (static_cast<std::size_t>(row) * mesh.columns + column) * kHeightmapVertexFloats;
    return {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
}

std::vector<std::uint8_t> filled(int width, int height, std::uint8_t level)
{
    return std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height, level);
}

// Level grows with the column, from 0 to 255.
std::vector<std::uint8_t> rampAlongX(int width, int height)
{
    std::vector<std::uint8_t> levels(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            levels[static_cast<std::size_t>(y) * width + x] = static_cast<std::uint8_t>(x * 255 / (width - 1));
    return levels;
}

} // namespace

TEST_CASE("Heightmap has one vertex per pixel and two triangles per cell", "[heightmap]")
{
    const auto mesh = dv::buildHeightmap(8, 6, filled(8, 6, 100));
    CHECK(mesh.columns == 8);
    CHECK(mesh.rows == 6);
    CHECK(mesh.step == 1);
    CHECK(mesh.vertices.size() == 8u * 6u * kHeightmapVertexFloats);
    CHECK(mesh.indices.size() == 7u * 5u * 6u);
    for (const auto index : mesh.indices)
        CHECK(index < 8u * 6u);
}

TEST_CASE("A flat image gives a flat mesh with upward normals", "[heightmap]")
{
    const auto mesh = dv::buildHeightmap(16, 16, filled(16, 16, 128));
    const float y0 = vertexAt(mesh, 0, 0).y;
    for (int row = 0; row < mesh.rows; ++row) {
        for (int column = 0; column < mesh.columns; ++column) {
            const Vertex v = vertexAt(mesh, column, row);
            CHECK(v.y == Approx(y0));
            CHECK(v.nx == Approx(0.0F).margin(1e-6));
            CHECK(v.ny == Approx(1.0F));
            CHECK(v.nz == Approx(0.0F).margin(1e-6));
        }
    }
}

TEST_CASE("Heights follow the levels and stay inside the extent", "[heightmap]")
{
    const auto mesh = dv::buildHeightmap(32, 4, rampAlongX(32, 4));
    float previous = -1e9F;
    for (int column = 0; column < mesh.columns; ++column) {
        const float y = vertexAt(mesh, column, 2).y;
        CHECK(y > previous);
        previous = y;
    }
    CHECK(vertexAt(mesh, 0, 0).y == Approx(-kHeightmapExtent / 2));
    CHECK(vertexAt(mesh, mesh.columns - 1, 0).y == Approx(kHeightmapExtent / 2));
    CHECK(mesh.boundsMin[1] == Approx(-kHeightmapExtent / 2));
    CHECK(mesh.boundsMax[1] == Approx(kHeightmapExtent / 2));
}

TEST_CASE("The footprint keeps the image aspect, row 0 at the back", "[heightmap]")
{
    const auto mesh = dv::buildHeightmap(40, 20, filled(40, 20, 0));
    const Vertex first = vertexAt(mesh, 0, 0);
    const Vertex last = vertexAt(mesh, mesh.columns - 1, mesh.rows - 1);
    CHECK(last.x - first.x == Approx(kHeightmapExtent));
    CHECK(last.z - first.z == Approx(kHeightmapExtent / 2));
    CHECK(first.z < last.z);
    CHECK(first.x < last.x);
}

TEST_CASE("Large images are decimated to the vertex budget", "[heightmap]")
{
    const int width = 1500;
    const int height = 700;
    const auto mesh = dv::buildHeightmap(width, height, filled(width, height, 10), {}, 512);
    CHECK(mesh.step == 3); // ceil(1500 / 512)
    CHECK(mesh.columns == 500);
    CHECK(mesh.rows == 234);
    CHECK(mesh.columns <= 512);
    CHECK(mesh.rows <= 512);
    CHECK(mesh.indices.size() == static_cast<std::size_t>(mesh.columns - 1) * (mesh.rows - 1) * 6);
}

TEST_CASE("Decimation averages the pixels of a cell", "[heightmap]")
{
    // 4x1 image decimated by 2: cells {0, 200} and {100, 100}.
    const std::vector<std::uint8_t> levels{0, 200, 100, 100};
    const auto mesh = dv::buildHeightmap(4, 1, levels, {}, 2);
    REQUIRE(mesh.step == 2);
    REQUIRE(mesh.columns == 2);
    CHECK(vertexAt(mesh, 0, 0).y == Approx((100.0F / 255.0F - 0.5F) * kHeightmapExtent));
    CHECK(vertexAt(mesh, 1, 0).y == Approx((100.0F / 255.0F - 0.5F) * kHeightmapExtent));
}

TEST_CASE("Normals are unit length and tilt away from the slope", "[heightmap]")
{
    const auto mesh = dv::buildHeightmap(32, 8, rampAlongX(32, 8));
    for (int row = 0; row < mesh.rows; ++row) {
        for (int column = 0; column < mesh.columns; ++column) {
            const Vertex v = vertexAt(mesh, column, row);
            CHECK(std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz) == Approx(1.0F));
            CHECK(v.ny > 0.0F);
        }
    }
    const Vertex middle = vertexAt(mesh, 16, 4);
    CHECK(middle.nx < 0.0F); // height rises towards +x, so the surface faces -x
    CHECK(middle.nz == Approx(0.0F).margin(1e-6));
}

TEST_CASE("Vertex colours come from the colour image, or the levels", "[heightmap]")
{
    const std::vector<std::uint8_t> levels{0, 255};
    const auto gray = dv::buildHeightmap(2, 1, levels);
    CHECK(vertexAt(gray, 0, 0).r == Approx(0.0F));
    CHECK(vertexAt(gray, 1, 0).g == Approx(1.0F));
    CHECK(vertexAt(gray, 1, 0).a == Approx(1.0F));

    const std::vector<std::uint8_t> rgb{255, 0, 0, 0, 0, 255};
    const auto colour = dv::buildHeightmap(2, 1, levels, rgb);
    CHECK(vertexAt(colour, 0, 0).r == Approx(1.0F));
    CHECK(vertexAt(colour, 0, 0).b == Approx(0.0F));
    CHECK(vertexAt(colour, 1, 0).b == Approx(1.0F));
}

TEST_CASE("Degenerate sizes still give a drawable mesh", "[heightmap]")
{
    for (const auto [w, h] : {std::pair{1, 40}, std::pair{40, 1}, std::pair{1, 1}, std::pair{1, 5000}}) {
        INFO(w << "x" << h);
        const auto mesh = dv::buildHeightmap(w, h, filled(w, h, 90));
        REQUIRE(mesh.columns >= 2);
        REQUIRE(mesh.rows >= 2);
        CHECK(mesh.columns <= 512);
        CHECK(mesh.rows <= 512);
        CHECK(mesh.vertices.size() == static_cast<std::size_t>(mesh.columns) * mesh.rows * kHeightmapVertexFloats);
        CHECK(mesh.indices.size() == static_cast<std::size_t>(mesh.columns - 1) * (mesh.rows - 1) * 6);
        for (const float f : mesh.vertices)
            CHECK(std::isfinite(f));
    }
}

TEST_CASE("Invalid input gives an empty mesh", "[heightmap][error]")
{
    CHECK(dv::buildHeightmap(0, 5, {}).vertices.empty());
    CHECK(dv::buildHeightmap(4, 4, filled(3, 3, 0)).vertices.empty());
    CHECK(dv::buildHeightmap(4, 4, filled(4, 4, 0), filled(1, 1, 0)).vertices.empty());
}
