// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "VolumeGpu.h"

using Catch::Approx;

namespace {

dv::Volume rampVolume(int width, int height, int depth)
{
    dv::Volume volume{.width = width, .height = height, .depth = depth, .spacing = {0.5, 0.5, 2.0}};
    volume.voxels.resize(static_cast<std::size_t>(width) * height * depth);
    for (std::size_t i = 0; i < volume.voxels.size(); ++i)
        volume.voxels[i] = static_cast<float>(i);
    return volume;
}

} // namespace

TEST_CASE("A volume within budget is not downsampled", "[volume][gpu]")
{
    CHECK(dv::downsampleFactor(256, 256, 128, 2) == 1);
    CHECK(dv::downsampleFactor(1, 1, 1, 2) == 1);
}

TEST_CASE("The factor is the smallest that fits the byte budget", "[volume][gpu]")
{
    // 512^3 voxels of 2 bytes = 256 MiB > 128 MiB; halved it is 32 MiB.
    CHECK(dv::downsampleFactor(512, 512, 512, 2) == 2);
    // 100^3 * 2 = 2 000 000 bytes is over 1 000 000; 50^3 * 2 = 250 000 fits.
    CHECK(dv::downsampleFactor(100, 100, 100, 2, 1'000'000) == 2);
    // Factor 3 gives 34^3 * 2 = 78 608 (over 70 000), factor 4 gives 25^3 * 2 = 31 250.
    CHECK(dv::downsampleFactor(100, 100, 100, 2, 70'000) == 4);
    // Non-multiples round up: 101 / 2 = 51 cells per side.
    CHECK(dv::downsampleFactor(101, 101, 101, 2, 1'000'000) == 2);
}

TEST_CASE("The factor also keeps every side within the texture limit", "[volume][gpu]")
{
    CHECK(dv::downsampleFactor(4096, 4, 4, 2) == 2);
    CHECK(dv::downsampleFactor(2048, 2048, 4, 2, std::size_t{1} << 40) == 1);
    CHECK(dv::downsampleFactor(9000, 2, 2, 2, std::size_t{1} << 40, 2048) == 5);
}

TEST_CASE("Downsampling averages each block and rounds the size up", "[volume][gpu]")
{
    dv::Volume volume{.width = 3, .height = 2, .depth = 2, .spacing = {1.0, 2.0, 3.0}};
    volume.voxels = {0, 2, 10, 4, 6, 20, //
                     8, 10, 30, 12, 14, 40};
    const dv::Volume half = dv::downsample(volume, 2);
    REQUIRE(half.width == 2);
    REQUIRE(half.height == 1);
    REQUIRE(half.depth == 1);
    REQUIRE(half.voxels.size() == 2);
    CHECK(half.voxels[0] == Approx((0 + 2 + 4 + 6 + 8 + 10 + 12 + 14) / 8.0));
    CHECK(half.voxels[1] == Approx((10 + 20 + 30 + 40) / 4.0)); // edge block holds 4 voxels
    CHECK(half.spacing[0] == Approx(2.0));
    CHECK(half.spacing[1] == Approx(4.0));
    CHECK(half.spacing[2] == Approx(6.0));
}

TEST_CASE("Downsampling by 1 is the identity", "[volume][gpu]")
{
    const dv::Volume volume = rampVolume(4, 3, 2);
    const dv::Volume same = dv::downsample(volume, 1);
    CHECK(same.width == 4);
    CHECK(same.voxels == volume.voxels);
    CHECK(same.spacing == volume.spacing);
}

TEST_CASE("Packing maps the value range to 16 bits", "[volume][gpu]")
{
    dv::Volume volume{.width = 2, .height = 1, .depth = 2};
    volume.voxels = {-1000.0F, 0.0F, 1000.0F, 3000.0F};
    const dv::PackedVolume packed = dv::packVolume(volume);
    CHECK(packed.downsample == 1);
    CHECK(packed.valueMin == Approx(-1000.0));
    CHECK(packed.valueMax == Approx(3000.0));
    REQUIRE(packed.voxels.size() == 4);
    CHECK(packed.voxels[0] == 0);
    CHECK(packed.voxels[1] == 16384); // 0.25 * 65535 = 16383.75
    CHECK(packed.voxels[2] == 32768); // 0.5 * 65535 = 32767.5
    CHECK(packed.voxels[3] == 65535);
}

TEST_CASE("Packing a constant volume does not divide by zero", "[volume][gpu]")
{
    dv::Volume volume{.width = 2, .height = 2, .depth = 1};
    volume.voxels = {7.0F, 7.0F, 7.0F, 7.0F};
    const dv::PackedVolume packed = dv::packVolume(volume);
    CHECK(packed.valueMax > packed.valueMin);
    for (const auto v : packed.voxels)
        CHECK(v == 0);
}

TEST_CASE("Packing downsamples a volume over the budget and reports it", "[volume][gpu]")
{
    const dv::Volume volume = rampVolume(40, 40, 40);
    const dv::PackedVolume packed = dv::packVolume(volume, 40'000); // 40^3 * 2 = 128 000 > 40 000
    CHECK(packed.downsample == 2);
    CHECK(packed.width == 20);
    CHECK(packed.height == 20);
    CHECK(packed.depth == 20);
    CHECK(packed.spacing[0] == Approx(1.0));
    CHECK(packed.spacing[2] == Approx(4.0));
    CHECK(packed.voxels.size() == 20u * 20u * 20u);
    CHECK(packed.voxels.size() * sizeof(std::uint16_t) <= 40'000);
}
