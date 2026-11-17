#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "DicomFile.h"
#include "TestSupport.h"

using dv::ErrorCode;
using namespace dv::test;

namespace {

// Stored (pre-rescale) signed 16-bit sample read straight from the file bytes.
std::int16_t storedSample(const std::vector<std::uint8_t>& bytes, std::size_t index)
{
    const std::size_t at = pixelDataOffset(bytes) + 2 * index;
    return static_cast<std::int16_t>(bytes[at] | (bytes[at + 1] << 8));
}

} // namespace

TEST_CASE("CT values are stored * slope + intercept (Hounsfield units)", "[pixeldata]")
{
    const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
    REQUIRE(file.has_value());
    REQUIRE(file->info().rescaleSlope == 1.0);
    REQUIRE(file->info().rescaleIntercept == -1024.0);

    const auto bytes = readBytes(dataFile("CT_small.dcm"));
    const auto values = file->rawModalityValues(0);
    REQUIRE(values.has_value());
    REQUIRE(values->size() == 128u * 128u);

    for (const std::size_t index : {std::size_t{0}, std::size_t{64 * 128 + 64}, std::size_t{100 * 128 + 20}, values->size() - 1}) {
        CAPTURE(index);
        const float expected = static_cast<float>(storedSample(bytes, index)) - 1024.0F;
        CHECK((*values)[index] == expected);
        CHECK(file->valueAt(static_cast<int>(index % 128), static_cast<int>(index / 128), 0) == expected);
    }

    // The slice contains air (about -900 HU) and tissue/bone (above 0 HU).
    const auto [lo, hi] = std::ranges::minmax(*values);
    CHECK(lo < -800.0F);
    CHECK(hi > 100.0F);
}

TEST_CASE("signed pixel representation yields negative stored values", "[pixeldata]")
{
    const auto file = dv::DicomFile::load(dataFile("MR_small.dcm"));
    REQUIRE(file.has_value());
    REQUIRE(file->info().pixelRepresentation == 1);
    const auto bytes = readBytes(dataFile("MR_small.dcm"));
    const auto values = file->rawModalityValues(0);
    REQUIRE(values.has_value());
    for (std::size_t i = 0; i < values->size(); i += 97)
        CHECK((*values)[i] == static_cast<float>(storedSample(bytes, i)));
}

TEST_CASE("valueAt rejects coordinates and frames outside the image", "[pixeldata][error]")
{
    const auto file = dv::DicomFile::load(dataFile("MR_small.dcm"));
    REQUIRE(file.has_value());
    CHECK(file->valueAt(63, 63, 0).has_value());
    for (const auto [x, y, frame] : {std::tuple{-1, 0, 0}, {0, -1, 0}, {64, 0, 0}, {0, 64, 0}, {0, 0, 1}, {0, 0, -1}}) {
        const auto result = file->valueAt(x, y, frame);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == ErrorCode::FrameOutOfRange);
    }
    CHECK(file->rawModalityValues(1).error().code == ErrorCode::FrameOutOfRange);
}

TEST_CASE("modality values are not available for color images", "[pixeldata][error]")
{
    const auto file = dv::DicomFile::load(dataFile("SC_rgb_small_odd.dcm"));
    REQUIRE(file.has_value());
    CHECK(file->rawModalityValues(0).error().code == ErrorCode::UnsupportedPhotometric);
    CHECK(file->valueAt(0, 0, 0).error().code == ErrorCode::UnsupportedPhotometric);
}
