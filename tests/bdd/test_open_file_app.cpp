// SPDX-License-Identifier: GPL-3.0-only
// Step definitions for the viewer-level scenarios of tests/features/open_file.feature,
// run against DicomController (the state behind the UI).

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>

#include <memory>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
#include "LanguageManager.h"
#include "ShaderReference.h"
#include "TestSupport.h"

using namespace dv::test;

namespace {

void openAndWait(DicomController &viewer, const std::filesystem::path &path)
{
    viewer.open(urlOf(path));
    REQUIRE(pumpUntil([&] { return !viewer.busy(); }));
}

bool showsSampleErrors(const DicomController &viewer)
{
    const ErrorModel &errors = *viewer.errors();
    return errors.count() > 0 && errors.data(errors.index(0), ErrorModel::PreviewRole).toBool();
}

} // namespace

SCENARIO("Viewer shows a valid monochrome file with its dimensions", "[bdd][open_file][viewer]")
{
    GIVEN("the viewer is showing the empty state")
    {
        DicomController viewer;
        REQUIRE_FALSE(viewer.hasImage());

        WHEN("I open a valid monochrome DICOM file")
        {
            openAndWait(viewer, dataFile("CT_small.dcm"));

            THEN("the image is shown with the dimensions stored in the file")
            {
                REQUIRE(viewer.hasImage());
                const QImage image = viewer.frameStore()->get();
                CHECK(image.width() == 128);
                CHECK(image.height() == 128);
                CHECK(image.format() == QImage::Format_Grayscale8);
                CHECK(viewer.columns() == image.width());
                CHECK(viewer.rows() == image.height());
            }
        }
    }
}

SCENARIO("Viewer keeps the image when an invalid file is opened", "[bdd][open_file][viewer]")
{
    GIVEN("the viewer is showing a previously opened image")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        REQUIRE(viewer.hasImage());
        REQUIRE(viewer.errors()->count() == 0);
        const QImage before = viewer.frameStore()->get();

        WHEN("I open a file that is not a valid DICOM file")
        {
            const TempDir dir;
            writeBytes(dir / "notes.dcm", std::vector<std::uint8_t>(64, 'x'));
            openAndWait(viewer, dir / "notes.dcm");

            THEN("the errors list shows the real error instead of the sample errors")
            {
                REQUIRE(viewer.errors()->count() == 1);
                CHECK_FALSE(showsSampleErrors(viewer));
                CHECK(viewer.errors()->data(viewer.errors()->index(0), ErrorModel::TitleRole).toString()
                      == ErrorText::message(dv::ErrorCode::NotDicom));
            }
            THEN("the previously opened image is still displayed")
            {
                CHECK(viewer.hasImage());
                CHECK(viewer.fileName() == QStringLiteral("CT_small.dcm"));
                CHECK(viewer.frameStore()->get() == before);
            }
        }
    }
}

SCENARIO("New resets the viewer", "[bdd][open_file][viewer]")
{
    GIVEN("the viewer is showing an image with metadata and options changed")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        viewer.setWindowWidth(123);
        viewer.setInvert(true);
        REQUIRE(viewer.hasImage());
        REQUIRE(viewer.metadata()->rowCount() > 0);

        WHEN("I press New")
        {
            viewer.reset();

            THEN("the image, metadata and options are cleared")
            {
                CHECK_FALSE(viewer.hasImage());
                CHECK(viewer.frameStore()->get().isNull());
                CHECK(viewer.metadata()->rowCount() == 0);
                CHECK(viewer.windowWidth() == 0);
                CHECK_FALSE(viewer.invert());
            }
            THEN("the errors list shows the sample errors again")
            {
                CHECK(showsSampleErrors(viewer));
            }
        }
    }
}

SCENARIO("Viewer shows an RGB file in color", "[bdd][open_file][viewer]")
{
    WHEN("I open a valid RGB DICOM file")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("SC_rgb_small_odd.dcm"));

        THEN("the image is displayed in color")
        {
            REQUIRE(viewer.hasImage());
            const QImage image = viewer.frameStore()->get();
            CHECK(image.format() == QImage::Format_RGB888);
            CHECK(viewer.photometric() == QStringLiteral("RGB"));
            const QColor first = image.pixelColor(0, 0);
            CHECK((first.red() != first.green() || first.green() != first.blue()));
        }
    }
}

SCENARIO("Rotate and flip updates orientation labels", "[bdd][open_file][viewer]")
{
    GIVEN("a valid monochrome DICOM file with a known orientation is open")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        REQUIRE(viewer.hasImage());
        const QString left = viewer.orientationLeft();
        const QString right = viewer.orientationRight();
        const QString top = viewer.orientationTop();
        const QString bottom = viewer.orientationBottom();
        REQUIRE(!left.isEmpty());

        WHEN("I rotate the view 90 degrees clockwise")
        {
            viewer.rotateRight();

            THEN("each edge shows the label that was on the edge turned onto it")
            {
                CHECK(viewer.orientationLeft() == bottom);
                CHECK(viewer.orientationTop() == left);
                CHECK(viewer.orientationRight() == top);
                CHECK(viewer.orientationBottom() == right);
            }
        }
        WHEN("I flip the view horizontally")
        {
            viewer.setFlipHorizontal(true);

            THEN("the left and right labels swap and the others stay")
            {
                CHECK(viewer.orientationLeft() == right);
                CHECK(viewer.orientationRight() == left);
                CHECK(viewer.orientationTop() == top);
                CHECK(viewer.orientationBottom() == bottom);
            }
        }
        WHEN("I rotate right and flip vertically")
        {
            viewer.rotateRight();
            viewer.setFlipVertical(true);

            THEN("the rotation is applied first, then the vertical mirror")
            {
                CHECK(viewer.orientationLeft() == bottom);
                CHECK(viewer.orientationTop() == right);
                CHECK(viewer.orientationRight() == top);
                CHECK(viewer.orientationBottom() == left);
            }
        }
    }
}

SCENARIO("GPU and CPU render the same image", "[bdd][open_file][viewer][gpu]")
{
    GIVEN("a valid monochrome DICOM file is open with the CPU backend")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        REQUIRE(viewer.hasImage());
        REQUIRE(viewer.renderBackend() == DicomController::Cpu);
        viewer.setWindowCenter(-100);
        viewer.setWindowWidth(500);
        const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
        REQUIRE(file.has_value());
        const auto expected = dv::renderFrame(*file, 0, {.voi = {-100, 500}});
        REQUIRE(expected.has_value());
        REQUIRE(pumpUntil([&] { return viewer.frameStore()->get() == toQImage(*expected); }));
        const QImage cpu = viewer.frameStore()->get();

        WHEN("I switch to the GPU backend")
        {
            viewer.setRenderBackend(DicomController::Gpu);
            REQUIRE(pumpUntil([&] { return viewer.gpuActive(); }));

            THEN("the window state is unchanged")
            {
                CHECK(viewer.windowCenter() == -100);
                CHECK(viewer.windowWidth() == 500);
            }
            THEN("the GPU shader math gives the same gray levels as the CPU image, within one level")
            {
                const QImage packedImage = viewer.gpuFrameStore()->get();
                REQUIRE(packedImage.size() == cpu.size());
                dv::PackedFrame packed{.width = packedImage.width(), .height = packedImage.height()};
                for (int y = 0; y < packedImage.height(); ++y) {
                    const uchar *row = packedImage.constScanLine(y);
                    packed.rgba.insert(packed.rgba.end(), row, row + packedImage.width() * 4);
                }
                const dv::GpuParams params{.storedMin = viewer.storedMin(),
                                           .slope = viewer.rescaleSlope(),
                                           .intercept = viewer.rescaleIntercept(),
                                           .monochrome1 = viewer.monochrome1()};
                const auto gpu = shaderRender(packed, params, {.voi = {viewer.windowCenter(), viewer.windowWidth()}});
                REQUIRE(gpu.size() == static_cast<size_t>(cpu.width() * cpu.height()) * 3);
                int maxDiff = 0;
                for (int y = 0; y < cpu.height(); ++y)
                    for (int x = 0; x < cpu.width(); ++x)
                        maxDiff = std::max(maxDiff, std::abs(int(gpu[(static_cast<size_t>(y) * cpu.width() + x) * 3])
                                                             - qGray(cpu.pixel(x, y))));
                CHECK(maxDiff <= 1);
            }
        }
    }
}

SCENARIO("GPU unavailable falls back to CPU", "[bdd][open_file][viewer][gpu]")
{
    GIVEN("the GPU backend is chosen and a monochrome file is shown by the GPU")
    {
        DicomController viewer;
        viewer.setRenderBackend(DicomController::Gpu);
        openAndWait(viewer, dataFile("CT_small.dcm"));
        REQUIRE(viewer.gpuActive());

        WHEN("the scene reports that GPU rendering is unavailable")
        {
            viewer.reportGpuUnavailable(QStringLiteral("shader failed to compile"));

            THEN("the CPU backend is used and the image is rendered on the CPU")
            {
                CHECK(viewer.renderBackend() == DicomController::Cpu);
                CHECK_FALSE(viewer.gpuAvailable());
                REQUIRE(pumpUntil([&] { return !viewer.gpuActive(); }));
                CHECK_FALSE(viewer.frameStore()->get().isNull());
                CHECK(viewer.hasImage());
            }
            THEN("the errors list shows one info entry saying GPU rendering is unavailable")
            {
                REQUIRE(viewer.errors()->count() == 1);
                const auto row = viewer.errors()->index(0);
                CHECK(viewer.errors()->data(row, ErrorModel::SeverityRole).toString() == QStringLiteral("info"));
                CHECK(viewer.errors()->data(row, ErrorModel::TitleRole).toString()
                      == ErrorText::message(dv::ErrorCode::GpuUnavailable));
            }
            THEN("choosing the GPU backend again is refused")
            {
                viewer.setRenderBackend(DicomController::Gpu);
                CHECK(viewer.renderBackend() == DicomController::Cpu);
            }
        }
    }
}

SCENARIO("Switch language", "[bdd][open_file][viewer][language]")
{
    GIVEN("the interface language is English")
    {
        const IsolatedSettings isolated;
        QQmlEngine engine;
        LanguageManager menu(&engine); // the language menu sets LanguageManager.language
        menu.setLanguage(QStringLiteral("en"));
        // The "Frame %1/%2" overlay of the 2D view, as QML binds it with qsTr().
        QQmlComponent overlay(&engine);
        overlay.setData("import QtQml\nQtObject { property string text: qsTr(\"Frame %1/%2\") }",
                        QUrl(QStringLiteral("file:///View2D.qml")));
        const std::unique_ptr<QObject> label(overlay.create());
        REQUIRE(label != nullptr);
        REQUIRE(label->property("text").toString() == QStringLiteral("Frame %1/%2"));

        WHEN("I choose Portuguese (BR) from the language menu")
        {
            menu.setLanguage(QStringLiteral("pt_BR"));

            THEN("the interface strings are shown in Portuguese")
            {
                CHECK(label->property("text").toString() == QStringLiteral("Quadro %1/%2"));
                CHECK(QCoreApplication::translate("StatusBar", "No file open") == QStringLiteral("Nenhum arquivo aberto"));
            }
        }
    }
}
