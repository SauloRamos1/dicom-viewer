// DicomController with series folders, multi-frame volumes and the 3D heightmap.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
#include "TestSupport.h"
#include "Windowing.h"

using Catch::Approx;
using namespace dv::test;

namespace {

void waitIdle(const DicomController &controller)
{
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
}

void openSeriesAndWait(DicomController &controller, const std::filesystem::path &folder)
{
    controller.openSeries(urlOf(folder));
    REQUIRE(controller.busy());
    waitIdle(controller);
}

void openAndWait(DicomController &controller, const std::filesystem::path &path)
{
    controller.open(urlOf(path));
    REQUIRE(controller.busy());
    waitIdle(controller);
}

QString errorTitle(const DicomController &controller, int row = 0)
{
    return controller.errors()->data(controller.errors()->index(row), ErrorModel::TitleRole).toString();
}

QString errorSeverity(const DicomController &controller, int row = 0)
{
    return controller.errors()->data(controller.errors()->index(row), ErrorModel::SeverityRole).toString();
}

bool showsSampleErrors(const DicomController &controller)
{
    const ErrorModel &errors = *controller.errors();
    return errors.count() > 0 && errors.data(errors.index(0), ErrorModel::PreviewRole).toBool();
}

// Counts the emissions of a signal while it lives (QtTest is not linked).
struct SignalCount {
    template <typename Sender, typename Signal>
    SignalCount(Sender *sender, Signal signal)
    {
        QObject::connect(sender, signal, sender, [this] { ++n; });
    }
    int count() const { return n; }
    int n = 0;
};

// Waits for a heightmap newer than `previous` (null = any).
std::shared_ptr<const dv::HeightmapMesh> waitForMesh(const DicomController &controller,
                                                     const std::shared_ptr<const dv::HeightmapMesh> &previous = {})
{
    REQUIRE(pumpUntil([&] { return controller.heightmap() && controller.heightmap() != previous; }));
    return controller.heightmap();
}

// Height (y) of vertex `index` of a mesh.
float heightAt(const dv::HeightmapMesh &mesh, std::size_t index)
{
    return mesh.vertices[index * dv::kHeightmapVertexFloats + 1];
}

// What the 2D view shows for slice file `path` with the controller's window.
QImage expectedSlice(const std::filesystem::path &path, const DicomController &controller)
{
    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto rendered = dv::renderFrame(*file, 0, {.voi = {controller.windowCenter(), controller.windowWidth()}});
    REQUIRE(rendered.has_value());
    return toQImage(*rendered);
}

} // namespace

TEST_CASE("A series folder opens as a volume with its slices as frames", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 6);
    DicomController controller;
    int loaded = 0;
    QObject::connect(&controller, &DicomController::loaded, [&] { ++loaded; });

    openSeriesAndWait(controller, dir.path());

    REQUIRE(controller.hasImage());
    CHECK(loaded == 1);
    CHECK(controller.hasVolume());
    CHECK(controller.frameCount() == 6);
    CHECK(controller.frame() == 0);
    CHECK(controller.volumeSize() == QVector3D(128, 128, 6));
    CHECK(controller.volumeSpacing().z() == Approx(5.0));
    CHECK(controller.volumeSpacing().x() == Approx(0.661468));
    CHECK(controller.volumeDownsample() == 1);
    CHECK(controller.volumeValueMax() > controller.volumeValueMin());
    CHECK(controller.rows() == 128);
    CHECK(controller.fileName() == QString::fromStdString(dir.path().filename().string()));
    CHECK(controller.errors()->count() == 0);
    CHECK(controller.metadata()->rowCount() > 0);

    const auto volume = controller.volumeData();
    REQUIRE(volume);
    CHECK(volume->width == 128);
    CHECK(volume->height == 128);
    CHECK(volume->depth == 6);
    CHECK(volume->bytes.size() == 128 * 128 * 6 * 2);
}

TEST_CASE("The volume texture holds the slices in order", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 4);
    DicomController controller;
    openSeriesAndWait(controller, dir.path());

    const auto volume = controller.volumeData();
    REQUIRE(volume);
    const auto *texels = reinterpret_cast<const quint16 *>(volume->bytes.constData());
    // Pixel (0,0) holds a value that rises with the slice, so do its texels.
    const std::size_t sliceTexels = 128 * 128;
    for (int z = 1; z < 4; ++z)
        CHECK(texels[z * sliceTexels] > texels[(z - 1) * sliceTexels]);
}

TEST_CASE("The 2D view steps through the slices of a series", "[app][series]")
{
    const TempDir dir;
    const auto paths = writeCtSeries(dir.path(), 5);
    DicomController controller;
    openSeriesAndWait(controller, dir.path());

    const int revision = controller.imageRevision();
    controller.setFrame(3);
    REQUIRE(pumpUntil([&] { return controller.imageRevision() != revision; }));
    CHECK(controller.frame() == 3);
    CHECK(controller.frameStore()->get() == expectedSlice(paths[3], controller));
    CHECK(controller.frameStore()->get() != expectedSlice(paths[2], controller));

    controller.setFrame(99); // clamped to the last slice
    CHECK(controller.frame() == 4);
    controller.setFrame(-5);
    CHECK(controller.frame() == 0);
}

TEST_CASE("A directory given to open() opens as a series", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 3);
    DicomController controller;

    openAndWait(controller, dir.path());

    CHECK(controller.hasVolume());
    CHECK(controller.frameCount() == 3);
}

TEST_CASE("A multi-frame monochrome file opens as a volume", "[app][series]")
{
    const TempDir dir;
    writeCtMultiFrame(dir / "multi.dcm", 4);
    DicomController controller;

    openAndWait(controller, dir / "multi.dcm");

    CHECK(controller.hasVolume());
    CHECK(controller.frameCount() == 4);
    CHECK(controller.volumeSize() == QVector3D(128, 128, 4));
    CHECK(controller.volumeData()->bytes.size() == 128 * 128 * 4 * 2);
    CHECK(controller.errors()->count() == 0);
}

TEST_CASE("A single image and a colour multi-frame file have no volume", "[app][series]")
{
    DicomController controller;

    openAndWait(controller, dataFile("CT_small.dcm"));
    CHECK_FALSE(controller.hasVolume());
    CHECK(controller.volumeData() == nullptr);
    CHECK(controller.volumeSize() == QVector3D());

    openAndWait(controller, dataFile("SC_rgb_rle_2frame.dcm"));
    CHECK(controller.frameCount() == 2);
    CHECK_FALSE(controller.hasVolume());
}

TEST_CASE("Reset and a new single file clear the volume", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 3);
    DicomController controller;
    openSeriesAndWait(controller, dir.path());
    REQUIRE(controller.hasVolume());
    SignalCount changed(&controller, &DicomController::volumeChanged);

    openAndWait(controller, dataFile("CT_small.dcm"));
    CHECK_FALSE(controller.hasVolume());
    CHECK(controller.frameCount() == 1);
    CHECK(changed.count() >= 1);

    openSeriesAndWait(controller, dir.path());
    REQUIRE(controller.hasVolume());
    controller.reset();
    CHECK_FALSE(controller.hasVolume());
    CHECK(controller.volumeData() == nullptr);
    CHECK(controller.volumeDownsample() == 1);
    CHECK(controller.frameCount() == 0);
    CHECK(showsSampleErrors(controller));
}

TEST_CASE("An empty folder or one without DICOM files is an error and keeps the image", "[app][series][error]")
{
    const TempDir empty;
    const TempDir text;
    writeBytes(text / "notes.txt", {});
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    const QImage before = controller.frameStore()->get();

    for (const auto &folder : {empty.path(), text.path()}) {
        INFO(folder.string());
        openSeriesAndWait(controller, folder);
        CHECK(controller.hasImage());
        CHECK(controller.fileName() == QStringLiteral("CT_small.dcm"));
        CHECK(controller.frameStore()->get() == before);
        REQUIRE(controller.errors()->count() == 1);
        CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::NoDicomFiles));
        CHECK(errorSeverity(controller) == QStringLiteral("error"));
    }
}

TEST_CASE("A missing folder is a file-not-found error", "[app][series][error]")
{
    DicomController controller;
    controller.openSeries(urlOf(std::filesystem::temp_directory_path() / "dicomviewer_no_such_folder"));
    waitIdle(controller);
    CHECK_FALSE(controller.hasImage());
    REQUIRE(controller.errors()->count() == 1);
    CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::FileNotFound));
}

TEST_CASE("Slices of different sizes are an inconsistent volume", "[app][series][error]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 2);
    rewrite(dataFile("MR_small.dcm"), dir / "slice_002.dcm", [](DcmDataset &ds) {
        ds.putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.100");
        ds.putAndInsertString(DCM_ImagePositionPatient, "0\\0\\10");
    });
    DicomController controller;

    openSeriesAndWait(controller, dir.path());

    CHECK_FALSE(controller.hasImage());
    REQUIRE(controller.errors()->count() == 1);
    CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::VolumeInconsistent));
    CHECK(controller.errors()->data(controller.errors()->index(0), ErrorModel::DetailRole).toString().contains(
        QStringLiteral("slice_002.dcm")));
}

TEST_CASE("A one-slice folder opens as a plain image with an info entry", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 1);
    DicomController controller;

    openSeriesAndWait(controller, dir.path());

    REQUIRE(controller.hasImage());
    CHECK_FALSE(controller.hasVolume());
    CHECK(controller.frameCount() == 1);
    REQUIRE(controller.errors()->count() == 1);
    CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::SingleSliceSeries));
    CHECK(errorSeverity(controller) == QStringLiteral("info"));
}

TEST_CASE("A new open abandons a series load in flight", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 40);
    DicomController controller;
    int loaded = 0;
    QObject::connect(&controller, &DicomController::loaded, [&] { ++loaded; });

    controller.openSeries(urlOf(dir.path()));
    controller.open(urlOf(dataFile("MR_small.dcm"))); // supersedes the series
    waitIdle(controller);
    pumpFor(300);

    CHECK(loaded == 1);
    CHECK(controller.fileName() == QStringLiteral("MR_small.dcm"));
    CHECK_FALSE(controller.hasVolume());
    CHECK(controller.frameCount() == 1);
}

TEST_CASE("New during a series load drops it", "[app][series]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 40);
    DicomController controller;

    controller.openSeries(urlOf(dir.path()));
    controller.reset();
    CHECK_FALSE(controller.busy());
    pumpFor(500);

    CHECK_FALSE(controller.hasImage());
    CHECK_FALSE(controller.hasVolume());
    CHECK(showsSampleErrors(controller));
}

TEST_CASE("A series follows the slice positions, not the file names", "[app][series]")
{
    // The lowest slice is renamed so that it sorts last by name.
    const TempDir dir;
    const auto paths = writeCtSeries(dir.path(), 3);
    std::filesystem::rename(paths[0], dir / "zz.dcm");
    DicomController controller;

    openSeriesAndWait(controller, dir.path());

    REQUIRE(controller.frameCount() == 3);
    CHECK(controller.frameStore()->get() == expectedSlice(dir / "zz.dcm", controller));
}

// ---- 3D mode and the heightmap ------------------------------------------------

TEST_CASE("The 3D view is off by default and toggles", "[app][3d]")
{
    DicomController controller;
    SignalCount changed(&controller, &DicomController::view3DChanged);
    CHECK_FALSE(controller.view3D());

    controller.setView3D(true);
    CHECK(controller.view3D());
    controller.setView3D(true); // no change, no signal
    controller.setView3D(false);
    CHECK_FALSE(controller.view3D());
    CHECK(changed.count() == 2);
}

TEST_CASE("A single image gets a heightmap only while the 3D view is shown", "[app][3d]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    pumpFor(100);
    CHECK(controller.heightmap() == nullptr);

    SignalCount changed(&controller, &DicomController::heightmapChanged);
    controller.setView3D(true);
    const auto mesh = waitForMesh(controller);
    REQUIRE(mesh);
    CHECK(mesh->columns == 128);
    CHECK(mesh->rows == 128);
    CHECK(mesh->step == 1);
    CHECK(mesh->indices.size() == 127u * 127u * 6u);
    CHECK(changed.count() >= 1);

    controller.setView3D(false);
    CHECK(controller.heightmap() == nullptr);
}

TEST_CASE("Opening a file while in 3D builds its heightmap", "[app][3d]")
{
    DicomController controller;
    controller.setView3D(true);
    CHECK(controller.heightmap() == nullptr);

    openAndWait(controller, dataFile("MR_small.dcm"));
    const auto mesh = waitForMesh(controller);
    CHECK(mesh->columns == controller.columns());
    CHECK(mesh->rows == controller.rows());

    controller.reset();
    CHECK(controller.heightmap() == nullptr);
}

TEST_CASE("The heightmap follows the window, the invert flag and the colormap", "[app][3d]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    controller.setView3D(true);
    auto mesh = waitForMesh(controller);

    // A narrower window changes the heights.
    controller.setWindowWidth(controller.windowWidth() / 4);
    auto next = waitForMesh(controller, mesh);
    CHECK(next->vertices != mesh->vertices);
    mesh = next;

    // Invert keeps the heights and flips the colours.
    controller.setInvert(true);
    next = waitForMesh(controller, mesh);
    std::size_t differentColour = 0;
    for (std::size_t i = 0; i < 128u * 128u; ++i) {
        CHECK(heightAt(*next, i) == heightAt(*mesh, i));
        differentColour += next->vertices[i * dv::kHeightmapVertexFloats + 6] != mesh->vertices[i * dv::kHeightmapVertexFloats + 6];
    }
    CHECK(differentColour > 1000);
    mesh = next;

    // A colormap colours the vertices (not gray any more) without moving them.
    controller.setColormap(DicomController::Jet);
    next = waitForMesh(controller, mesh);
    bool coloured = false;
    for (std::size_t i = 0; i < 128u * 128u; ++i) {
        CHECK(heightAt(*next, i) == heightAt(*mesh, i));
        const float *v = next->vertices.data() + i * dv::kHeightmapVertexFloats;
        coloured |= v[6] != v[8];
    }
    CHECK(coloured);
}

TEST_CASE("The heightmap follows the frame of a colour multi-frame image", "[app][3d]")
{
    DicomController controller;
    openAndWait(controller, dataFile("SC_rgb_rle_2frame.dcm"));
    controller.setView3D(true);
    const auto first = waitForMesh(controller);
    CHECK(first->vertices.size() == static_cast<std::size_t>(first->columns) * first->rows * dv::kHeightmapVertexFloats);

    controller.setFrame(1);
    const auto second = waitForMesh(controller, first);
    CHECK(second->vertices != first->vertices);
}

TEST_CASE("A file with a volume gets no heightmap", "[app][3d]")
{
    const TempDir dir;
    writeCtSeries(dir.path(), 3);
    DicomController controller;
    controller.setView3D(true);

    openSeriesAndWait(controller, dir.path());
    pumpFor(200);

    CHECK(controller.hasVolume());
    CHECK(controller.heightmap() == nullptr);
    controller.setWindowWidth(controller.windowWidth() / 2);
    pumpFor(200);
    CHECK(controller.heightmap() == nullptr);
}

TEST_CASE("Colour images take the luma as height and keep their colours", "[app][3d]")
{
    DicomController controller;
    openAndWait(controller, dataFile("SC_rgb_small_odd.dcm"));
    controller.setView3D(true);
    const auto mesh = waitForMesh(controller);

    // The first vertex averages one pixel when the image is small enough to keep every pixel.
    REQUIRE(mesh->step == 1);
    const float *first = mesh->vertices.data();
    CHECK(first[6] != first[7]); // the colour of the first pixel is not gray
    const float luma = 0.299F * first[6] + 0.587F * first[7] + 0.114F * first[8];
    CHECK(first[1] == Approx((luma - 0.5F) * dv::kHeightmapExtent).margin(0.5));
}
