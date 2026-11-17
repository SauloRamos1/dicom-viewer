// SPDX-License-Identifier: GPL-3.0-only
// The controller's pixel geometry: display aspect, spacing text and the
// value under a screen point.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSizeF>
#include <QVariantMap>

#include <utility>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "TestSupport.h"

using Catch::Approx;
using namespace dv::test;

namespace {

void openAndWait(DicomController &controller, const std::filesystem::path &path)
{
    controller.open(urlOf(path));
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
    REQUIRE(controller.hasImage());
}

// The pixel under the centre of image pixel (x, y) when the viewport is the image
// at one screen pixel per image pixel (square pixels).
QVariantMap pixelAtCentre(const DicomController &controller, int x, int y)
{
    return controller.pixelAt(QPointF(x + 0.5, y + 0.5), QSizeF(controller.columns(), controller.rows()), 1.0, QPointF());
}

QString textOf(const QVariantMap &pixel)
{
    return pixel.value(QStringLiteral("text")).toString();
}

std::pair<int, int> coordsOf(const QVariantMap &pixel)
{
    return {pixel.value(QStringLiteral("x")).toInt(), pixel.value(QStringLiteral("y")).toInt()};
}

std::filesystem::path nonSquareCt(const TempDir &dir, const char *spacing)
{
    rewrite(dataFile("CT_small.dcm"), dir / "non_square.dcm",
            [&](DcmDataset &ds) { ds.putAndInsertString(DCM_PixelSpacing, spacing); });
    return dir / "non_square.dcm";
}

} // namespace

TEST_CASE("The controller exposes the pixel aspect and the spacing text", "[app][controller][pixel]")
{
    DicomController controller;
    CHECK(controller.pixelAspect() == 1.0);
    CHECK(controller.pixelSpacingText().isEmpty());

    SECTION("square pixels")
    {
        openAndWait(controller, dataFile("CT_small.dcm"));
        const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
        REQUIRE(file.has_value());
        REQUIRE(file->info().pixelSpacing.has_value());
        CHECK(controller.pixelAspect() == Approx(file->info().pixelSpacing->at(1) / file->info().pixelSpacing->at(0)));
        CHECK(controller.pixelSpacingText().endsWith(QStringLiteral(" mm")));
    }
    SECTION("Pixel Spacing 0.5 \\ 1.0 is twice as wide as tall")
    {
        const TempDir dir;
        openAndWait(controller, nonSquareCt(dir, "0.5\\1.0"));
        CHECK(controller.pixelAspect() == Approx(2.0));
        CHECK(controller.pixelSpacingText() == QStringLiteral("0.5 x 1.0 mm"));
    }
    SECTION("Imager Pixel Spacing stands in for a missing Pixel Spacing")
    {
        const TempDir dir;
        rewrite(dataFile("CT_small.dcm"), dir / "imager.dcm", [](DcmDataset &ds) {
            ds.findAndDeleteElement(DCM_PixelSpacing);
            ds.putAndInsertString(DCM_ImagerPixelSpacing, "0.25\\0.125");
        });
        openAndWait(controller, dir / "imager.dcm");
        CHECK(controller.pixelAspect() == Approx(0.5));
        CHECK(controller.pixelSpacingText() == QStringLiteral("0.25 x 0.125 mm"));
    }
    SECTION("Pixel Aspect Ratio alone gives the aspect but no spacing text")
    {
        const TempDir dir;
        rewrite(dataFile("CT_small.dcm"), dir / "ratio.dcm", [](DcmDataset &ds) {
            ds.findAndDeleteElement(DCM_PixelSpacing);
            ds.putAndInsertString(DCM_PixelAspectRatio, "2\\1");
        });
        openAndWait(controller, dir / "ratio.dcm");
        CHECK(controller.pixelAspect() == Approx(0.5));
        CHECK(controller.pixelSpacingText().isEmpty());
    }
    SECTION("a file without any pixel size information")
    {
        const TempDir dir;
        rewrite(dataFile("CT_small.dcm"), dir / "none.dcm", [](DcmDataset &ds) { ds.findAndDeleteElement(DCM_PixelSpacing); });
        openAndWait(controller, dir / "none.dcm");
        CHECK(controller.pixelAspect() == 1.0);
        CHECK(controller.pixelSpacingText().isEmpty());
    }
    SECTION("New clears them")
    {
        const TempDir dir;
        openAndWait(controller, nonSquareCt(dir, "0.5\\1.0"));
        controller.reset();
        CHECK(controller.pixelAspect() == 1.0);
        CHECK(controller.pixelSpacingText().isEmpty());
    }
}

TEST_CASE("The pixel under the cursor reports its position and modality value", "[app][controller][pixel]")
{
    DicomController controller;
    CHECK(controller.pixelAt(QPointF(1, 1), QSizeF(128, 128), 1.0, QPointF()).isEmpty()); // no image

    openAndWait(controller, dataFile("CT_small.dcm"));
    const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
    REQUIRE(file.has_value());
    REQUIRE(file->info().modality == "CT");

    for (const auto &[x, y] : {std::pair{0, 0}, std::pair{64, 64}, std::pair{127, 3}}) {
        INFO("pixel " << x << "," << y);
        const auto value = file->valueAt(x, y, 0);
        REQUIRE(value.has_value());
        REQUIRE(*value == static_cast<float>(static_cast<int>(*value))); // CT values are whole HU
        const QVariantMap pixel = pixelAtCentre(controller, x, y);
        REQUIRE_FALSE(pixel.isEmpty());
        CHECK(coordsOf(pixel) == std::pair{x, y});
        CHECK(textOf(pixel) == QStringLiteral("%1 HU").arg(static_cast<int>(*value)));
    }
}

TEST_CASE("The readout is empty outside the image", "[app][controller][pixel]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    const QSizeF viewport(256, 128); // the 128 x 128 image is centred with 64 px bars each side
    CHECK(controller.pixelAt(QPointF(10, 64), viewport, 1.0, QPointF()).isEmpty());
    CHECK(controller.pixelAt(QPointF(250, 64), viewport, 1.0, QPointF()).isEmpty());
    CHECK_FALSE(controller.pixelAt(QPointF(128, 64), viewport, 1.0, QPointF()).isEmpty());
    CHECK(controller.pixelAt(QPointF(128, -1), viewport, 1.0, QPointF()).isEmpty());
}

TEST_CASE("The readout follows pan, zoom, rotation and flips", "[app][controller][pixel]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    const QSizeF viewport(128, 128);
    const auto pixelAt = [&](double sx, double sy, double zoom = 1.0, QPointF pan = {}) {
        return coordsOf(controller.pixelAt(QPointF(sx, sy), viewport, zoom, pan));
    };

    CHECK(pixelAt(10.5, 20.5) == std::pair{10, 20});
    CHECK(pixelAt(74.5, 84.5, 1.0, QPointF(64, 64)) == std::pair{10, 20}); // panned
    CHECK(pixelAt(64.5, 64.5, 2.0) == std::pair{64, 64});                  // zoomed about the centre

    controller.setFlipHorizontal(true);
    CHECK(pixelAt(10.5, 20.5) == std::pair{117, 20});
    controller.setFlipHorizontal(false);

    controller.rotateRight(); // clockwise: the top-left pixel is now at the top-right corner
    CHECK(pixelAt(127.5, 0.5) == std::pair{0, 0});
    controller.setFlipVertical(true); // then mirrored: it moves to the bottom-right corner
    CHECK(pixelAt(127.5, 127.5) == std::pair{0, 0});
}

TEST_CASE("The readout accounts for the pixel aspect", "[app][controller][pixel]")
{
    const TempDir dir;
    DicomController controller;
    openAndWait(controller, nonSquareCt(dir, "0.5\\1.0")); // twice as wide as tall: 256 x 128 on screen

    const QSizeF viewport(256, 128);
    CHECK(coordsOf(controller.pixelAt(QPointF(2 * 10 + 1, 20.5), viewport, 1.0, QPointF())) == std::pair{10, 20});
    CHECK(coordsOf(controller.pixelAt(QPointF(255.5, 127.5), viewport, 1.0, QPointF())) == std::pair{127, 127});

    // In a 128 x 128 viewport the image is 128 x 64, centred: the bars above and below are outside.
    const QSizeF square(128, 128);
    CHECK(controller.pixelAt(QPointF(64, 10), square, 1.0, QPointF()).isEmpty());
    CHECK(coordsOf(controller.pixelAt(QPointF(64, 64), square, 1.0, QPointF())) == std::pair{64, 64});
}

TEST_CASE("The readout shows no unit outside CT and R G B for colour images", "[app][controller][pixel]")
{
    DicomController controller;

    SECTION("MR")
    {
        openAndWait(controller, dataFile("MR_small.dcm"));
        const auto file = dv::DicomFile::load(dataFile("MR_small.dcm"));
        REQUIRE(file.has_value());
        const auto value = file->valueAt(5, 7, 0);
        REQUIRE(value.has_value());
        const QString text = textOf(pixelAtCentre(controller, 5, 7));
        CHECK_FALSE(text.contains(QStringLiteral("HU")));
        CHECK(text == QString::number(static_cast<double>(*value), 'f', 0));
    }
    SECTION("RGB")
    {
        openAndWait(controller, dataFile("SC_rgb_small_odd.dcm"));
        const QImage image = controller.frameStore()->get();
        REQUIRE(image.format() == QImage::Format_RGB888);
        for (const auto &[x, y] : {std::pair{0, 0}, std::pair{2, 1}}) {
            const QColor colour = image.pixelColor(x, y);
            const QVariantMap pixel = pixelAtCentre(controller, x, y);
            REQUIRE_FALSE(pixel.isEmpty());
            CHECK(textOf(pixel)
                  == QStringLiteral("R %1  G %2  B %3").arg(colour.red()).arg(colour.green()).arg(colour.blue()));
        }
    }
}

TEST_CASE("The readout reads the current frame", "[app][controller][pixel]")
{
    const TempDir dir;
    writeCtMultiFrame(dir / "multi.dcm", 3);
    DicomController controller;
    openAndWait(controller, dir / "multi.dcm");
    const auto file = dv::DicomFile::load(dir / "multi.dcm");
    REQUIRE(file.has_value());

    for (const int frame : {0, 2}) {
        controller.setFrame(frame);
        const auto value = file->valueAt(0, 0, frame);
        REQUIRE(value.has_value());
        CHECK(textOf(pixelAtCentre(controller, 0, 0)) == QStringLiteral("%1 HU").arg(static_cast<int>(*value)));
    }
}

TEST_CASE("An inverted colour image still reads its own values", "[app][controller][pixel]")
{
    DicomController controller;
    openAndWait(controller, dataFile("SC_rgb_small_odd.dcm"));
    const QString before = textOf(pixelAtCentre(controller, 0, 0));
    const int revision = controller.imageRevision();
    controller.setInvert(true);
    REQUIRE(pumpUntil([&] { return controller.imageRevision() != revision; }));
    CHECK(textOf(pixelAtCentre(controller, 0, 0)) == before);
}
