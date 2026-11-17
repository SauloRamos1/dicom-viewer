#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

#include "DicomFile.h"
#include "Orientation.h"
#include "TestSupport.h"

using dv::OrientationLabels;

namespace {

OrientationLabels labelsOf(const std::array<double, 6>& iop)
{
    const auto labels = dv::orientationLabels(iop);
    REQUIRE(labels.has_value());
    return *labels;
}

} // namespace

TEST_CASE("axial orientation: R on the left, A on top", "[orientation]")
{
    CHECK(labelsOf({1, 0, 0, 0, 1, 0}) == OrientationLabels{.left = "R", .right = "L", .top = "A", .bottom = "P"});
}

TEST_CASE("sagittal orientation: A on the left, H on top", "[orientation]")
{
    CHECK(labelsOf({0, 1, 0, 0, 0, -1}) == OrientationLabels{.left = "A", .right = "P", .top = "H", .bottom = "F"});
}

TEST_CASE("coronal orientation: R on the left, H on top", "[orientation]")
{
    CHECK(labelsOf({1, 0, 0, 0, 0, -1}) == OrientationLabels{.left = "R", .right = "L", .top = "H", .bottom = "F"});
}

TEST_CASE("oblique orientation combines letters by significance", "[orientation]")
{
    // Axial plane rotated about the patient's head-feet axis.
    const auto labels = labelsOf({0.9701, 0.2425, 0, -0.2425, 0.9701, 0});
    CHECK(labels.right == "LP");
    CHECK(labels.left == "RA");
    CHECK(labels.bottom == "PR");
    CHECK(labels.top == "AL");
}

TEST_CASE("components below the threshold are ignored", "[orientation]")
{
    // A 5 degree tilt keeps the pure labels.
    const double s = std::sin(5.0 * M_PI / 180.0);
    const double c = std::cos(5.0 * M_PI / 180.0);
    CHECK(labelsOf({c, s, 0, -s, c, 0}) == OrientationLabels{.left = "R", .right = "L", .top = "A", .bottom = "P"});
}

TEST_CASE("a degenerate orientation has no labels", "[orientation]")
{
    CHECK_FALSE(dv::orientationLabels(std::array<double, 6>{0, 0, 0, 0, 1, 0}).has_value());
    CHECK_FALSE(dv::orientationLabels(std::array<double, 6>{1, 0, 0, 0, 0, 0}).has_value());
}

TEST_CASE("labels come from the file's orientation, none when the tag is missing", "[orientation]")
{
    const auto ct = dv::DicomFile::load(dv::test::dataFile("CT_small.dcm"));
    REQUIRE(ct.has_value());
    REQUIRE(ct->info().imageOrientation.has_value());
    CHECK(dv::orientationLabels(ct->info()).has_value());

    const auto rgb = dv::DicomFile::load(dv::test::dataFile("SC_rgb_small_odd.dcm"));
    REQUIRE(rgb.has_value());
    REQUIRE_FALSE(rgb->info().imageOrientation.has_value());
    CHECK_FALSE(dv::orientationLabels(rgb->info()).has_value());
}
