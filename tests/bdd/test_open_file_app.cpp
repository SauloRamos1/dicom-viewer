// Step definitions for the viewer-level scenarios of tests/features/open_file.feature,
// run against DicomController (the state behind the UI).

#include <catch2/catch_test_macros.hpp>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
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
