// Step definitions for tests/features/view_3d.feature, run against DicomController.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
#include "TestSupport.h"

using Catch::Approx;
using namespace dv::test;

namespace {

void openAndWait(DicomController &viewer, const std::filesystem::path &path)
{
    viewer.open(urlOf(path));
    REQUIRE(pumpUntil([&] { return !viewer.busy(); }));
}

void openSeriesAndWait(DicomController &viewer, const std::filesystem::path &folder)
{
    viewer.openSeries(urlOf(folder));
    REQUIRE(pumpUntil([&] { return !viewer.busy(); }));
}

} // namespace

SCENARIO("Heightmap for a single image", "[bdd][view_3d]")
{
    GIVEN("a single-frame image is open")
    {
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        REQUIRE(viewer.hasImage());

        WHEN("I switch to the 3D view")
        {
            viewer.setView3D(true);
            REQUIRE(pumpUntil([&] { return viewer.heightmap() != nullptr; }));

            THEN("a heightmap of the image is built with one vertex per pixel")
            {
                const auto mesh = viewer.heightmap();
                CHECK(mesh->columns == viewer.columns());
                CHECK(mesh->rows == viewer.rows());
                CHECK(mesh->vertices.size() == static_cast<std::size_t>(128 * 128) * dv::kHeightmapVertexFloats);
            }
            AND_THEN("the 3D view has no volume")
            {
                CHECK_FALSE(viewer.hasVolume());
                CHECK(viewer.volumeData() == nullptr);
            }
        }
    }
}

SCENARIO("Volume from a series folder", "[bdd][view_3d]")
{
    GIVEN("a folder with the slices of one series")
    {
        const TempDir dir;
        writeCtSeries(dir.path(), 8);
        DicomController viewer;

        WHEN("I open the folder as a series")
        {
            openSeriesAndWait(viewer, dir.path());

            THEN("the series is a volume with the slice count as depth and the slice spacing as z spacing")
            {
                REQUIRE(viewer.hasVolume());
                CHECK(viewer.volumeSize() == QVector3D(128, 128, 8));
                CHECK(viewer.volumeSpacing().z() == Approx(5.0));
            }
            AND_THEN("the 2D view shows the slices as frames")
            {
                CHECK(viewer.frameCount() == 8);
                viewer.setFrame(7);
                CHECK(viewer.frame() == 7);
            }
        }
    }
}

SCENARIO("Inconsistent series shows an error", "[bdd][view_3d]")
{
    GIVEN("a folder whose slices have different sizes")
    {
        const TempDir dir;
        writeCtSeries(dir.path(), 2);
        rewrite(dataFile("MR_small.dcm"), dir / "slice_002.dcm", [](DcmDataset &ds) {
            ds.putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.100");
            ds.putAndInsertString(DCM_ImagePositionPatient, "0\\0\\10");
        });
        DicomController viewer;
        openAndWait(viewer, dataFile("CT_small.dcm"));
        const QImage before = viewer.frameStore()->get();

        WHEN("I open the folder as a series")
        {
            openSeriesAndWait(viewer, dir.path());

            THEN("the errors list shows that the slices do not form a consistent volume")
            {
                REQUIRE(viewer.errors()->count() == 1);
                CHECK(viewer.errors()->data(viewer.errors()->index(0), ErrorModel::TitleRole).toString()
                      == ErrorText::message(dv::ErrorCode::VolumeInconsistent));
            }
            AND_THEN("the previously opened image is still displayed")
            {
                CHECK(viewer.fileName() == QStringLiteral("CT_small.dcm"));
                CHECK(viewer.frameStore()->get() == before);
                CHECK_FALSE(viewer.hasVolume());
            }
        }
    }
}

SCENARIO("Multi-frame file opens as a volume", "[bdd][view_3d]")
{
    GIVEN("a multi-frame monochrome file")
    {
        const TempDir dir;
        writeCtMultiFrame(dir / "multi.dcm", 5);
        DicomController viewer;

        WHEN("I open it")
        {
            openAndWait(viewer, dir / "multi.dcm");

            THEN("it is a volume with the frame count as depth")
            {
                REQUIRE(viewer.hasVolume());
                CHECK(viewer.volumeSize().z() == 5.0F);
                CHECK(viewer.frameCount() == 5);
            }
        }
    }
}
