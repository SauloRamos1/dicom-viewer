#include <catch2/catch_test_macros.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
#include "TestSupport.h"
#include "Windowing.h"

using namespace dv::test;

namespace {

// Opens `path` and waits for the result.
void openAndWait(DicomController &controller, const std::filesystem::path &path)
{
    controller.open(urlOf(path));
    REQUIRE(controller.busy());
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
}

QImage shown(const DicomController &controller)
{
    return controller.frameStore()->get();
}

// Waits until a render started by a property change has been shown.
void waitForRender(const DicomController &controller, int previousRevision)
{
    REQUIRE(pumpUntil([&] { return controller.imageRevision() != previousRevision; }));
}

QString errorTitle(const DicomController &controller, int row = 0)
{
    return controller.errors()->data(controller.errors()->index(row), ErrorModel::TitleRole).toString();
}

QImage expectedFrame(const std::filesystem::path &path, int frame, const dv::RenderParams &params)
{
    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto rendered = dv::renderFrame(*file, frame, params);
    REQUIRE(rendered.has_value());
    return toQImage(*rendered);
}

} // namespace

TEST_CASE("Controller starts empty with the sample errors", "[app][controller]")
{
    const DicomController controller;
    CHECK_FALSE(controller.hasImage());
    CHECK_FALSE(controller.busy());
    CHECK_FALSE(controller.windowable());
    CHECK(controller.fileName().isEmpty());
    CHECK(controller.frameCount() == 0);
    CHECK(controller.errors()->count() == 3);
    CHECK(controller.metadata()->rowCount() == 0);
    CHECK(shown(controller).isNull());
}

TEST_CASE("Opening a monochrome file shows it with the values stored in the file", "[app][controller]")
{
    DicomController controller;
    int loaded = 0;
    QObject::connect(&controller, &DicomController::loaded, [&] { ++loaded; });
    const auto path = dataFile("CT_small.dcm");

    openAndWait(controller, path);

    REQUIRE(controller.hasImage());
    CHECK(loaded == 1);
    CHECK(controller.fileName() == QStringLiteral("CT_small.dcm"));
    CHECK(controller.filePath() == QString::fromStdString(path.string()));
    CHECK(controller.rows() == 128);
    CHECK(controller.columns() == 128);
    CHECK(controller.bitsStored() == 16);
    CHECK(controller.photometric() == QStringLiteral("MONOCHROME2"));
    CHECK(controller.modality() == QStringLiteral("CT"));
    CHECK_FALSE(controller.transferSyntax().isEmpty());
    CHECK(controller.frameCount() == 1);
    CHECK(controller.frame() == 0);
    CHECK(controller.windowable());
    CHECK(controller.errors()->count() == 0);
    CHECK(controller.metadata()->rowCount() > 20);
    CHECK(controller.imageRevision() > 0);
    CHECK(controller.lastRenderMs() > 0);

    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto voi = dv::presetVoi(dv::WindowPreset::FromFile, *file);
    REQUIRE(voi.has_value());
    CHECK(controller.windowCenter() == voi->center);
    CHECK(controller.windowWidth() == voi->width);
    CHECK(controller.patientName() == QString::fromStdString(file->info().patientName));
    CHECK(controller.patientId() == QString::fromStdString(file->info().patientId));
    if (const auto labels = dv::orientationLabels(file->info())) {
        CHECK(controller.orientationLeft() == QString::fromStdString(labels->left));
        CHECK(controller.orientationTop() == QString::fromStdString(labels->top));
    }

    const QImage image = shown(controller);
    CHECK(image.size() == QSize(128, 128));
    CHECK(image.format() == QImage::Format_Grayscale8);
    CHECK(image == expectedFrame(path, 0, {.voi = *voi}));
}

TEST_CASE("A failed load keeps the previous image, metadata and window", "[app][controller]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    REQUIRE(controller.hasImage());
    controller.setWindowWidth(500);
    controller.setWindowCenter(-100);
    waitForRender(controller, 0);
    REQUIRE(pumpUntil([&] { return shown(controller) == expectedFrame(dataFile("CT_small.dcm"), 0, {.voi = {-100, 500}}); }));

    const int revision = controller.imageRevision();
    const QImage image = shown(controller);
    const int metadataRows = controller.metadata()->rowCount();
    int failures = 0;
    QObject::connect(&controller, &DicomController::errorOccurred, [&] { ++failures; });

    SECTION("not a DICOM file")
    {
        const TempDir dir;
        writeBytes(dir / "random.dcm", std::vector<std::uint8_t>(300, 0x5A));
        openAndWait(controller, dir / "random.dcm");
        CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::NotDicom));
    }
    SECTION("unsupported transfer syntax")
    {
        openAndWait(controller, dataFile("JPEG2000.dcm"));
        CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::UnsupportedTransferSyntax));
    }
    SECTION("missing file")
    {
        openAndWait(controller, dataFile("does_not_exist.dcm"));
        CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::FileNotFound));
    }

    CHECK(failures == 1);
    CHECK(controller.errors()->count() == 1);
    CHECK_FALSE(controller.errors()->data(controller.errors()->index(0), ErrorModel::PreviewRole).toBool());
    CHECK(controller.hasImage());
    CHECK(controller.fileName() == QStringLiteral("CT_small.dcm"));
    CHECK(controller.imageRevision() == revision);
    CHECK(shown(controller) == image);
    CHECK(controller.metadata()->rowCount() == metadataRows);
    CHECK(controller.windowCenter() == -100);
    CHECK(controller.windowWidth() == 500);
    CHECK_FALSE(controller.busy());
}

TEST_CASE("A successful load replaces the errors and the view settings", "[app][controller]")
{
    DicomController controller;
    openAndWait(controller, dataFile("does_not_exist.dcm"));
    REQUIRE(controller.errors()->count() == 1);
    openAndWait(controller, dataFile("CT_small.dcm"));
    controller.setInvert(true);
    controller.setColormap(DicomController::Jet);
    REQUIRE(controller.errors()->count() == 0);

    openAndWait(controller, dataFile("MR_small.dcm"));

    CHECK(controller.fileName() == QStringLiteral("MR_small.dcm"));
    CHECK(controller.errors()->count() == 0);
    CHECK_FALSE(controller.invert());
    CHECK(controller.colormap() == DicomController::Gray);
}

TEST_CASE("Reset returns to the empty state and the sample errors", "[app][controller]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    REQUIRE(controller.hasImage());

    controller.reset();

    CHECK_FALSE(controller.hasImage());
    CHECK(controller.fileName().isEmpty());
    CHECK(controller.rows() == 0);
    CHECK(controller.frameCount() == 0);
    CHECK(controller.windowWidth() == 0);
    CHECK(controller.metadata()->rowCount() == 0);
    CHECK(shown(controller).isNull());
    REQUIRE(controller.errors()->count() == 3);
    CHECK(controller.errors()->data(controller.errors()->index(0), ErrorModel::PreviewRole).toBool());
}

TEST_CASE("Reset cancels a load in flight", "[app][controller]")
{
    DicomController controller;
    controller.open(urlOf(dataFile("CT_small.dcm")));
    controller.reset();
    CHECK_FALSE(controller.busy());

    pumpFor(300);
    CHECK_FALSE(controller.hasImage());
    CHECK(controller.errors()->count() == 3);
}

TEST_CASE("Only the latest of two quick opens is applied", "[app][controller]")
{
    DicomController controller;
    int loaded = 0;
    QObject::connect(&controller, &DicomController::loaded, [&] { ++loaded; });

    controller.open(urlOf(dataFile("CT_small.dcm")));
    controller.open(urlOf(dataFile("MR_small.dcm")));
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
    pumpFor(300); // the superseded result must not arrive late and win

    CHECK(loaded == 1);
    CHECK(controller.fileName() == QStringLiteral("MR_small.dcm"));
    CHECK(controller.rows() == 64);
}

TEST_CASE("Changing the window re-renders and updates the render time", "[app][controller]")
{
    DicomController controller;
    const auto path = dataFile("CT_small.dcm");
    openAndWait(controller, path);
    const int revision = controller.imageRevision();
    const QImage before = shown(controller);
    int windowChanges = 0;
    QObject::connect(&controller, &DicomController::windowChanged, [&] { ++windowChanges; });

    SECTION("a single change")
    {
        controller.setWindowWidth(80);
        controller.setWindowCenter(40);
        waitForRender(controller, revision);
        REQUIRE(pumpUntil([&] { return shown(controller) == expectedFrame(path, 0, {.voi = {40, 80}}); }));
        CHECK(shown(controller) != before);
        CHECK(controller.lastRenderMs() > 0);
    }

    SECTION("rapid changes are coalesced and the last one wins")
    {
        for (int i = 1; i <= 30; ++i)
            controller.setWindowCenter(i * 10.0);
        const dv::RenderParams last{.voi = {300, controller.windowWidth()}};
        REQUIRE(pumpUntil([&] { return shown(controller) == expectedFrame(path, 0, last); }));
        pumpFor(100);
        CHECK(shown(controller) == expectedFrame(path, 0, last));
        CHECK(controller.imageRevision() - revision < 30);
        CHECK(windowChanges == 30);
    }

    SECTION("width is kept at 1 or more and no-op changes do not render")
    {
        controller.setWindowWidth(-5);
        CHECK(controller.windowWidth() == 1);
        controller.setWindowWidth(1);
        controller.setWindowCenter(controller.windowCenter());
        waitForRender(controller, revision); // only the first change rendered
        pumpFor(100);
        CHECK(controller.imageRevision() == revision + 1);
    }
}

TEST_CASE("Window presets and reset", "[app][controller]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    const double defaultCenter = controller.windowCenter();
    const double defaultWidth = controller.windowWidth();

    const int revision = controller.imageRevision();
    controller.applyPreset(QStringLiteral("lung"));
    waitForRender(controller, revision);
    pumpFor(100);
    CHECK(controller.imageRevision() == revision + 1); // width and level render together
    CHECK(controller.windowCenter() == dv::kCtLung.center);
    CHECK(controller.windowWidth() == dv::kCtLung.width);
    controller.applyPreset(QStringLiteral("brain"));
    CHECK(controller.windowCenter() == dv::kCtBrain.center);
    controller.applyPreset(QStringLiteral("nonsense"));
    CHECK(controller.windowCenter() == dv::kCtBrain.center);

    controller.resetWindow();
    CHECK(controller.windowCenter() == defaultCenter);
    CHECK(controller.windowWidth() == defaultWidth);

    controller.applyPreset(QStringLiteral("minmax"));
    CHECK(controller.windowWidth() > 1);
}

TEST_CASE("Frames of a multi-frame colour file", "[app][controller]")
{
    DicomController controller;
    const auto path = dataFile("SC_rgb_rle_2frame.dcm");
    openAndWait(controller, path);

    REQUIRE(controller.hasImage());
    CHECK(controller.frameCount() == 2);
    CHECK(controller.photometric() == QStringLiteral("RGB"));
    CHECK_FALSE(controller.windowable());
    const QImage first = shown(controller);
    CHECK(first.format() == QImage::Format_RGB888);
    CHECK(first == expectedFrame(path, 0, {}));

    const int revision = controller.imageRevision();
    controller.setFrame(1);
    waitForRender(controller, revision);
    CHECK(controller.frame() == 1);
    CHECK(shown(controller) == expectedFrame(path, 1, {}));
    CHECK(shown(controller) != first);

    controller.setFrame(7); // clamped
    CHECK(controller.frame() == 1);
    controller.setFrame(-3);
    CHECK(controller.frame() == 0);

    // The window does not apply to colour images.
    controller.setWindowWidth(10);
    CHECK(controller.windowWidth() == 0);
}

TEST_CASE("Invert and colormap change the rendered pixels", "[app][controller]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "mono1.dcm",
            [](DcmDataset &ds) { REQUIRE(ds.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1").good()); });
    DicomController controller;
    openAndWait(controller, dir / "mono1.dcm");
    REQUIRE(controller.photometric() == QStringLiteral("MONOCHROME1"));
    const QImage normal = shown(controller);
    const int revision = controller.imageRevision();

    controller.setInvert(true);
    waitForRender(controller, revision);
    const QImage inverted = shown(controller);
    REQUIRE(inverted.size() == normal.size());
    CHECK(inverted != normal);
    for (int y = 0; y < normal.height(); y += 7) {
        for (int x = 0; x < normal.width(); x += 7)
            REQUIRE(qGray(inverted.pixel(x, y)) == 255 - qGray(normal.pixel(x, y)));
    }

    const int revision2 = controller.imageRevision();
    controller.setColormap(DicomController::Jet);
    waitForRender(controller, revision2);
    CHECK(shown(controller).format() == QImage::Format_RGB888);
}

TEST_CASE("A file without pixel data shows its metadata and an error", "[app][controller]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "nopixels.dcm",
            [](DcmDataset &ds) { ds.findAndDeleteElement(DCM_PixelData); });
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    int failures = 0;
    QObject::connect(&controller, &DicomController::errorOccurred, [&] { ++failures; });

    openAndWait(controller, dir / "nopixels.dcm");

    CHECK(failures == 1);
    CHECK_FALSE(controller.hasImage());
    CHECK(controller.fileName() == QStringLiteral("nopixels.dcm"));
    CHECK(shown(controller).isNull());
    CHECK(controller.metadata()->rowCount() > 20);
    REQUIRE(controller.errors()->count() == 1);
    CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::NoPixelData));
    CHECK(controller.errors()->data(controller.errors()->index(0), ErrorModel::TagRole).toString()
          == QStringLiteral("(7FE0,0010)"));
}

TEST_CASE("The metadata filter works on the controller's model", "[app][controller]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    const int all = controller.metadata()->rowCount();

    controller.metadata()->setFilterText(QStringLiteral("modality"));
    CHECK(controller.metadata()->rowCount() > 0);
    CHECK(controller.metadata()->rowCount() < all);

    controller.reset();
    CHECK(controller.metadata()->rowCount() == 0);
}
