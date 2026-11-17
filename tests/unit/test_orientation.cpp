// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <numbers>

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
    const double s = std::sin(5.0 * std::numbers::pi / 180.0);
    const double c = std::cos(5.0 * std::numbers::pi / 180.0);
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

namespace {

// Axial base: R on the left, L on the right, A on top, P at the bottom.
const OrientationLabels kAxial{.left = "R", .right = "L", .top = "A", .bottom = "P"};

} // namespace

TEST_CASE("an untouched view keeps the labels", "[orientation][transform]")
{
    CHECK(dv::transformLabels(kAxial, {}) == kAxial);
}

TEST_CASE("labels follow every rotation and flip combination", "[orientation][transform]")
{
    struct Case {
        dv::ViewTransform transform;
        OrientationLabels expected;
    };
    // Rotation is clockwise and applied first, flips mirror the result on screen.
    const std::array<Case, 16> cases{{
        {{0, false, false}, {.left = "R", .right = "L", .top = "A", .bottom = "P"}},
        {{0, true, false}, {.left = "L", .right = "R", .top = "A", .bottom = "P"}},
        {{0, false, true}, {.left = "R", .right = "L", .top = "P", .bottom = "A"}},
        {{0, true, true}, {.left = "L", .right = "R", .top = "P", .bottom = "A"}},
        {{1, false, false}, {.left = "P", .right = "A", .top = "R", .bottom = "L"}},
        {{1, true, false}, {.left = "A", .right = "P", .top = "R", .bottom = "L"}},
        {{1, false, true}, {.left = "P", .right = "A", .top = "L", .bottom = "R"}},
        {{1, true, true}, {.left = "A", .right = "P", .top = "L", .bottom = "R"}},
        {{2, false, false}, {.left = "L", .right = "R", .top = "P", .bottom = "A"}},
        {{2, true, false}, {.left = "R", .right = "L", .top = "P", .bottom = "A"}},
        {{2, false, true}, {.left = "L", .right = "R", .top = "A", .bottom = "P"}},
        {{2, true, true}, {.left = "R", .right = "L", .top = "A", .bottom = "P"}},
        {{3, false, false}, {.left = "A", .right = "P", .top = "L", .bottom = "R"}},
        {{3, true, false}, {.left = "P", .right = "A", .top = "L", .bottom = "R"}},
        {{3, false, true}, {.left = "A", .right = "P", .top = "R", .bottom = "L"}},
        {{3, true, true}, {.left = "P", .right = "A", .top = "R", .bottom = "L"}},
    }};
    for (const Case& c : cases) {
        CAPTURE(c.transform.quarterTurns, c.transform.flipHorizontal, c.transform.flipVertical);
        CHECK(dv::transformLabels(kAxial, c.transform) == c.expected);
    }
}

TEST_CASE("rotation counts wrap around, in both directions", "[orientation][transform]")
{
    CHECK(dv::transformLabels(kAxial, {4, false, false}) == kAxial);
    CHECK(dv::transformLabels(kAxial, {5, false, false}) == dv::transformLabels(kAxial, {1, false, false}));
    CHECK(dv::transformLabels(kAxial, {-1, false, false}) == dv::transformLabels(kAxial, {3, false, false}));
    CHECK(dv::transformLabels(kAxial, {-6, false, false}) == dv::transformLabels(kAxial, {2, false, false}));
}

TEST_CASE("oblique labels move as whole strings", "[orientation][transform]")
{
    const OrientationLabels oblique{.left = "RA", .right = "LP", .top = "AL", .bottom = "PR"};
    CHECK(dv::transformLabels(oblique, {1, false, false})
          == OrientationLabels{.left = "PR", .right = "AL", .top = "RA", .bottom = "LP"});
}
