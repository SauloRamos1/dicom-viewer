// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <utility>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "DicomFile.h"
#include "TestSupport.h"
#include "ViewGeometry.h"

using Catch::Approx;
using namespace dv;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

ImageInfo withSpacing(double row, double column)
{
    ImageInfo info;
    info.pixelSpacing = {{row, column}};
    return info;
}

} // namespace

TEST_CASE("Pixel aspect is 1 when the file says nothing about the pixel size", "[viewgeometry]")
{
    CHECK(pixelAspect(ImageInfo{}) == 1.0);
    CHECK_FALSE(physicalPixelSpacing(ImageInfo{}).has_value());
}

TEST_CASE("Pixel aspect is column spacing over row spacing", "[viewgeometry]")
{
    // DICOM order: {row spacing, column spacing}. Row spacing is the pixel height.
    CHECK(pixelAspect(withSpacing(0.5, 1.0)) == Approx(2.0)); // twice as wide as tall
    CHECK(pixelAspect(withSpacing(1.0, 0.5)) == Approx(0.5));
    CHECK(pixelAspect(withSpacing(0.7, 0.7)) == Approx(1.0));
}

TEST_CASE("Pixel aspect falls back to Imager Pixel Spacing, then Pixel Aspect Ratio", "[viewgeometry]")
{
    ImageInfo info;
    info.pixelAspectRatio = {{1.0, 3.0}}; // vertical size : horizontal size
    CHECK(pixelAspect(info) == Approx(3.0));
    info.pixelAspectRatio = {{2.0, 1.0}};
    CHECK(pixelAspect(info) == Approx(0.5));

    info.imagerPixelSpacing = {{0.2, 0.1}};
    CHECK(pixelAspect(info) == Approx(0.5)); // Imager Pixel Spacing beats the ratio

    info.pixelSpacing = {{0.5, 1.0}};
    CHECK(pixelAspect(info) == Approx(2.0)); // Pixel Spacing beats both
}

TEST_CASE("The physical spacing is Pixel Spacing, else Imager Pixel Spacing, never the ratio", "[viewgeometry]")
{
    ImageInfo info;
    info.pixelAspectRatio = {{1.0, 3.0}};
    CHECK_FALSE(physicalPixelSpacing(info).has_value());

    info.imagerPixelSpacing = {{0.2, 0.1}};
    REQUIRE(physicalPixelSpacing(info).has_value());
    CHECK((*physicalPixelSpacing(info))[0] == Approx(0.2));
    CHECK((*physicalPixelSpacing(info))[1] == Approx(0.1));

    info.pixelSpacing = {{0.5, 1.0}};
    REQUIRE(physicalPixelSpacing(info).has_value());
    CHECK((*physicalPixelSpacing(info))[0] == Approx(0.5));
    CHECK((*physicalPixelSpacing(info))[1] == Approx(1.0));
}

TEST_CASE("Zero, negative and non-finite values never give a pixel aspect other than 1", "[viewgeometry]")
{
    for (const auto& [row, column] : {std::pair{0.0, 1.0}, std::pair{1.0, 0.0}, std::pair{-0.5, 1.0},
                                      std::pair{1.0, -2.0}, std::pair{kNaN, 1.0}, std::pair{1.0, kNaN},
                                      std::pair{kInf, 1.0}, std::pair{1.0, kInf}, std::pair{0.0, 0.0}}) {
        INFO("row " << row << " column " << column);
        CHECK(pixelAspect(withSpacing(row, column)) == 1.0);
        CHECK_FALSE(physicalPixelSpacing(withSpacing(row, column)).has_value());
    }

    ImageInfo ratio;
    ratio.pixelAspectRatio = {{0.0, 2.0}};
    CHECK(pixelAspect(ratio) == 1.0);
    ratio.pixelAspectRatio = {{1.0, -2.0}};
    CHECK(pixelAspect(ratio) == 1.0);

    // An unusable Pixel Spacing does not hide a usable fallback.
    ImageInfo info = withSpacing(0.0, 1.0);
    info.imagerPixelSpacing = {{0.2, 0.1}};
    CHECK(pixelAspect(info) == Approx(0.5));
}

TEST_CASE("Pixel aspect is read from the file", "[viewgeometry][dicomfile]")
{
    SECTION("Pixel Spacing")
    {
        const test::TempDir dir;
        test::rewrite(test::dataFile("CT_small.dcm"), dir / "ps.dcm",
                      [](DcmDataset& ds) { ds.putAndInsertString(DCM_PixelSpacing, "0.5\\1.0"); });
        const auto file = DicomFile::load(dir / "ps.dcm");
        REQUIRE(file.has_value());
        CHECK(pixelAspect(file->info()) == Approx(2.0));
    }
    SECTION("Imager Pixel Spacing when there is no Pixel Spacing")
    {
        const test::TempDir dir;
        test::rewrite(test::dataFile("CT_small.dcm"), dir / "imager.dcm", [](DcmDataset& ds) {
            ds.findAndDeleteElement(DCM_PixelSpacing);
            ds.putAndInsertString(DCM_ImagerPixelSpacing, "0.2\\0.1");
        });
        const auto file = DicomFile::load(dir / "imager.dcm");
        REQUIRE(file.has_value());
        CHECK(pixelAspect(file->info()) == Approx(0.5));
    }
    SECTION("Pixel Aspect Ratio when there is no spacing at all")
    {
        const test::TempDir dir;
        test::rewrite(test::dataFile("CT_small.dcm"), dir / "ratio.dcm", [](DcmDataset& ds) {
            ds.findAndDeleteElement(DCM_PixelSpacing);
            ds.putAndInsertString(DCM_PixelAspectRatio, "1\\2");
        });
        const auto file = DicomFile::load(dir / "ratio.dcm");
        REQUIRE(file.has_value());
        CHECK(pixelAspect(file->info()) == Approx(2.0));
        CHECK_FALSE(physicalPixelSpacing(file->info()).has_value());
    }
    SECTION("a file with square pixels")
    {
        const auto file = DicomFile::load(test::dataFile("CT_small.dcm"));
        REQUIRE(file.has_value());
        CHECK(pixelAspect(file->info()) == Approx(1.0));
    }
}

namespace {

// 400 x 200 viewport showing a 4 x 2 pixel image with square pixels: unturned it
// fills the viewport (100 screen pixels per image pixel); turned a quarter it is
// 100 x 200 in the middle (50 per image pixel).
ViewMapping baseView()
{
    return {.viewportWidth = 400, .viewportHeight = 200, .columns = 4, .rows = 2};
}

} // namespace

TEST_CASE("Screen points map to image pixels with fit, no zoom and no pan", "[viewgeometry]")
{
    const ViewMapping view = baseView();
    const auto at = [&](double x, double y) { return screenToPixel(view, x, y); };

    REQUIRE(at(0, 0).has_value());
    CHECK(*at(0, 0) == PixelPos{0, 0});
    CHECK(*at(399.9, 199.9) == PixelPos{3, 1});
    CHECK(*at(150, 50) == PixelPos{1, 0});
    CHECK(*at(250, 150) == PixelPos{2, 1});
    CHECK_FALSE(at(400, 100).has_value());
    CHECK_FALSE(at(-0.1, 100).has_value());
    CHECK_FALSE(at(100, 200).has_value());
    CHECK_FALSE(at(100, -1).has_value());
}

TEST_CASE("Letterboxing around the fitted image is outside the image", "[viewgeometry]")
{
    ViewMapping view = baseView();
    view.viewportWidth = 800; // 4 x 2 image in 800 x 200: 400 x 200 box centred, 200 px bars each side
    const auto at = [&](double x, double y) { return screenToPixel(view, x, y); };
    CHECK_FALSE(at(100, 100).has_value());
    CHECK_FALSE(at(199, 100).has_value());
    CHECK(*at(200, 0) == PixelPos{0, 0});
    CHECK(*at(599, 199) == PixelPos{3, 1});
    CHECK_FALSE(at(600, 100).has_value());
}

TEST_CASE("Zoom is about the viewport centre and pan moves the image", "[viewgeometry]")
{
    ViewMapping view = baseView();
    view.zoom = 2; // each image pixel is 200 screen pixels; the box spans 800 x 400 about (200, 100)
    CHECK(*screenToPixel(view, 200, 100) == PixelPos{2, 1}); // the centre is the corner shared by the middle pixels
    CHECK(*screenToPixel(view, 199, 99) == PixelPos{1, 0});
    CHECK(*screenToPixel(view, 0, 0) == PixelPos{1, 0});
    CHECK(*screenToPixel(view, 399, 199) == PixelPos{2, 1});
    CHECK(*screenToPixel(view, 0, 199) == PixelPos{1, 1});

    // Panned 200 px right and 100 px up: the box centre is at (400, 0), so it spans
    // x 0..800 and y -200..200.
    view.panX = 200;
    view.panY = -100;
    CHECK(*screenToPixel(view, 1, 1) == PixelPos{0, 1});
    CHECK(*screenToPixel(view, 399, 1) == PixelPos{1, 1});
    CHECK_FALSE(screenToPixel(view, 1, 201).has_value());
}

TEST_CASE("Zooming out shows the whole image in a smaller box", "[viewgeometry]")
{
    ViewMapping view = baseView();
    view.zoom = 0.5; // box 200 x 100 centred: x 100..300, y 50..150
    CHECK_FALSE(screenToPixel(view, 99, 100).has_value());
    CHECK(*screenToPixel(view, 100, 50) == PixelPos{0, 0});
    CHECK(*screenToPixel(view, 299, 149) == PixelPos{3, 1});
    CHECK_FALSE(screenToPixel(view, 300, 100).has_value());
}

TEST_CASE("A non-square pixel stretches the image box before it is fitted", "[viewgeometry]")
{
    SECTION("narrow pixels (aspect 0.5): a 100 x 100 image is half as wide as tall")
    {
        const ViewMapping view{.viewportWidth = 400, .viewportHeight = 200, .columns = 100, .rows = 100,
                               .pixelAspect = 0.5};
        // display aspect 0.5: box 100 x 200 centred: x 150..250
        CHECK_FALSE(screenToPixel(view, 149, 100).has_value());
        CHECK(*screenToPixel(view, 150, 0) == PixelPos{0, 0});
        CHECK(*screenToPixel(view, 249, 199) == PixelPos{99, 99});
        CHECK(*screenToPixel(view, 200, 100) == PixelPos{50, 50});
        CHECK_FALSE(screenToPixel(view, 250, 100).has_value());
    }
    SECTION("wide pixels (aspect 2): the box is as wide as the viewport")
    {
        const ViewMapping view{.viewportWidth = 400, .viewportHeight = 200, .columns = 100, .rows = 100,
                               .pixelAspect = 2.0};
        CHECK(*screenToPixel(view, 0, 0) == PixelPos{0, 0});
        CHECK(*screenToPixel(view, 399, 199) == PixelPos{99, 99});
        CHECK(*screenToPixel(view, 200, 100) == PixelPos{50, 50});
        CHECK(*screenToPixel(view, 100, 50) == PixelPos{25, 25});
    }
    SECTION("rotated a quarter turn the stretched box still fits the turned viewport")
    {
        // 100 x 100 with aspect 2 is 2:1; turned, it is 1:2 and fits 400 x 200 as 100 x 200 at x 150..250.
        const ViewMapping view{.viewportWidth = 400, .viewportHeight = 200, .columns = 100, .rows = 100,
                               .pixelAspect = 2.0, .transform = {.quarterTurns = 1}};
        CHECK_FALSE(screenToPixel(view, 149, 100).has_value());
        CHECK(*screenToPixel(view, 249.5, 1) == PixelPos{0, 0}); // top-left pixel is at the top-right corner
        CHECK(*screenToPixel(view, 150.5, 199) == PixelPos{99, 99});
    }
}

namespace {

// Where the screen puts the centre of image pixel (px, py): an independent
// forward model of the viewer (turn clockwise, then mirror, on a fitted box).
std::pair<double, double> forward(const ViewMapping& view, int px, int py)
{
    const double displayAspect = view.columns * view.pixelAspect / view.rows;
    const bool sideways = view.transform.quarterTurns % 2 != 0;
    const double rotatorWidth = sideways ? view.viewportHeight : view.viewportWidth;
    const double rotatorHeight = sideways ? view.viewportWidth : view.viewportHeight;
    const double boxWidth = std::min(rotatorWidth, rotatorHeight * displayAspect);
    const double boxHeight = boxWidth / displayAspect;
    double u = (px + 0.5) / view.columns * boxWidth - boxWidth / 2;
    double v = (py + 0.5) / view.rows * boxHeight - boxHeight / 2;
    for (int i = 0; i < view.transform.quarterTurns; ++i) { // clockwise on a y-down screen
        const double turnedU = -v;
        v = u;
        u = turnedU;
    }
    if (view.transform.flipHorizontal)
        u = -u;
    if (view.transform.flipVertical)
        v = -v;
    return {view.viewportWidth / 2 + view.panX + u * view.zoom, view.viewportHeight / 2 + view.panY + v * view.zoom};
}

} // namespace

TEST_CASE("Every rotation and flip maps each screen pixel back to the image pixel drawn there", "[viewgeometry]")
{
    for (int quarterTurns = 0; quarterTurns < 4; ++quarterTurns) {
        for (const bool flipH : {false, true}) {
            for (const bool flipV : {false, true}) {
                for (const double aspect : {1.0, 0.5, 2.0}) {
                    for (const double zoom : {1.0, 2.5}) {
                        ViewMapping view = baseView();
                        view.columns = 6;
                        view.rows = 3;
                        view.pixelAspect = aspect;
                        view.zoom = zoom;
                        view.panX = 17;
                        view.panY = -9;
                        view.transform = {.quarterTurns = quarterTurns, .flipHorizontal = flipH, .flipVertical = flipV};
                        INFO("turns " << quarterTurns << " flipH " << flipH << " flipV " << flipV << " aspect " << aspect
                                      << " zoom " << zoom);
                        for (int py = 0; py < view.rows; ++py) {
                            for (int px = 0; px < view.columns; ++px) {
                                const auto [sx, sy] = forward(view, px, py);
                                const auto pixel = screenToPixel(view, sx, sy);
                                REQUIRE(pixel.has_value());
                                CHECK(*pixel == PixelPos{px, py});
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("The first and last pixel land in the expected screen corners for every orientation", "[viewgeometry]")
{
    // Turn clockwise, then mirror. `right` / `bottom`: the screen corner of pixel (0,0); pixel
    // (3,1) is in the opposite one. Viewport 400 x 200, 4 x 2 square pixels: unturned the image
    // fills it, turned a quarter it is the 100 x 200 strip x 150..250.
    struct Case {
        int quarterTurns;
        bool flipH;
        bool flipV;
        bool right;
        bool bottom;
    };
    const Case cases[] = {
        {0, false, false, false, false}, {1, false, false, true, false}, {2, false, false, true, true},
        {3, false, false, false, true},  {0, true, false, true, false},  {1, true, false, false, false},
        {2, true, false, false, true},   {3, true, false, true, true},   {0, false, true, false, true},
        {1, false, true, true, true},    {2, false, true, true, false},  {3, false, true, false, false},
        {0, true, true, true, true},     {1, true, true, false, true},   {2, true, true, false, false},
        {3, true, true, true, false},
    };
    for (const Case& c : cases) {
        ViewMapping view = baseView();
        view.transform = {.quarterTurns = c.quarterTurns, .flipHorizontal = c.flipH, .flipVertical = c.flipV};
        const bool sideways = c.quarterTurns % 2 != 0;
        const double left = sideways ? 151 : 1;
        const double right = sideways ? 249 : 399;
        const double top = 1;
        const double bottom = 199;
        INFO("turns " << c.quarterTurns << " flipH " << c.flipH << " flipV " << c.flipV);
        const auto first = screenToPixel(view, c.right ? right : left, c.bottom ? bottom : top);
        REQUIRE(first.has_value());
        CHECK(*first == PixelPos{0, 0});
        const auto last = screenToPixel(view, c.right ? left : right, c.bottom ? top : bottom);
        REQUIRE(last.has_value());
        CHECK(*last == PixelPos{3, 1});
    }
}

TEST_CASE("Degenerate views map nothing", "[viewgeometry]")
{
    CHECK_FALSE(screenToPixel(ViewMapping{}, 0, 0).has_value()); // no image, no viewport
    ViewMapping view = baseView();
    view.zoom = 0;
    CHECK_FALSE(screenToPixel(view, 100, 100).has_value());
    view = baseView();
    view.pixelAspect = kNaN;
    CHECK_FALSE(screenToPixel(view, 100, 100).has_value());
    view = baseView();
    CHECK_FALSE(screenToPixel(view, kNaN, 100).has_value());
}
