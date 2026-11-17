#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "TestSupport.h"
#include "Volume.h"

using dv::ErrorCode;
using namespace dv::test;

namespace {

struct SliceSpec {
    std::optional<double> z;     // Image Position (Patient) z; omitted when empty
    int instance{1};
    std::string seriesUid{"1.2.3.100"};
    int marker{0};               // stored value of pixel (0,0), identifies the slice
};

// Writes a CT_small based slice with the given position, instance and marker.
std::filesystem::path writeSlice(const TempDir& dir, std::string_view name, const SliceSpec& spec)
{
    const auto path = dir / name;
    rewrite(dataFile("CT_small.dcm"), path, [&](DcmDataset& ds) {
        ds.putAndInsertString(DCM_ImageOrientationPatient, "1\\0\\0\\0\\1\\0");
        if (spec.z)
            ds.putAndInsertString(DCM_ImagePositionPatient, ("0\\0\\" + std::to_string(*spec.z)).c_str());
        else
            ds.findAndDeleteElement(DCM_ImagePositionPatient);
        ds.putAndInsertString(DCM_InstanceNumber, std::to_string(spec.instance).c_str());
        ds.putAndInsertString(DCM_SeriesInstanceUID, spec.seriesUid.c_str());
        const Uint16* pixels = nullptr;
        unsigned long count = 0;
        REQUIRE(ds.findAndGetUint16Array(DCM_PixelData, pixels, &count).good());
        std::vector<Uint16> copy(pixels, pixels + count);
        copy[0] = static_cast<Uint16>(spec.marker);
        REQUIRE(ds.putAndInsertUint16Array(DCM_PixelData, copy.data(), count).good());
    });
    return path;
}

float firstVoxel(const dv::Volume& volume, int z)
{
    return volume.voxels[static_cast<std::size_t>(z) * static_cast<std::size_t>(volume.width * volume.height)];
}

} // namespace

TEST_CASE("fromSeries sorts slices by position along the slice normal", "[volume]")
{
    const TempDir dir;
    // Files given out of order, and instance numbers that disagree with positions.
    const std::vector<std::filesystem::path> paths{
        writeSlice(dir, "a.dcm", {.z = 10.0, .instance = 1, .marker = 10}),
        writeSlice(dir, "b.dcm", {.z = 0.0, .instance = 3, .marker = 0}),
        writeSlice(dir, "c.dcm", {.z = 5.0, .instance = 2, .marker = 5}),
    };

    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE(volume.has_value());
    CHECK(volume->width == 128);
    CHECK(volume->height == 128);
    CHECK(volume->depth == 3);
    CHECK(volume->voxels.size() == 128u * 128u * 3u);
    CHECK(volume->spacing[0] == Catch::Approx(0.661468));
    CHECK(volume->spacing[2] == Catch::Approx(5.0));

    // Values are modality values: stored marker + intercept (-1024).
    CHECK(firstVoxel(*volume, 0) == 0.0F - 1024.0F);
    CHECK(firstVoxel(*volume, 1) == 5.0F - 1024.0F);
    CHECK(firstVoxel(*volume, 2) == 10.0F - 1024.0F);
}

TEST_CASE("fromSeries falls back to the instance number without positions", "[volume]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{
        writeSlice(dir, "a.dcm", {.instance = 3, .marker = 30}),
        writeSlice(dir, "b.dcm", {.instance = 1, .marker = 10}),
        writeSlice(dir, "c.dcm", {.instance = 2, .marker = 20}),
    };

    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE(volume.has_value());
    REQUIRE(volume->depth == 3);
    CHECK(firstVoxel(*volume, 0) == 10.0F - 1024.0F);
    CHECK(firstVoxel(*volume, 1) == 20.0F - 1024.0F);
    CHECK(firstVoxel(*volume, 2) == 30.0F - 1024.0F);
    CHECK(volume->spacing[2] == Catch::Approx(5.0)); // slice thickness
}

TEST_CASE("fromSeries keeps only the first file's series and skips non-DICOM files", "[volume]")
{
    const TempDir dir;
    writeBytes(dir / "notes.txt", {});
    const std::vector<std::filesystem::path> paths{
        dir / "notes.txt",
        writeSlice(dir, "a.dcm", {.z = 0.0, .marker = 1}),
        writeSlice(dir, "other.dcm", {.z = 5.0, .seriesUid = "9.9.9", .marker = 2}),
        writeSlice(dir, "b.dcm", {.z = 5.0, .marker = 3}),
    };

    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE(volume.has_value());
    CHECK(volume->depth == 2);
    CHECK(firstVoxel(*volume, 1) == 3.0F - 1024.0F);
}

TEST_CASE("fromSeries rejects slices of different sizes", "[volume][error]")
{
    const TempDir dir;
    const auto ct = writeSlice(dir, "a.dcm", {.z = 0.0});
    const auto mr = dir / "mr.dcm";
    rewrite(dataFile("MR_small.dcm"), mr, [](DcmDataset& ds) {
        ds.putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.100");
        ds.putAndInsertString(DCM_ImagePositionPatient, "0\\0\\5");
    });

    const std::vector<std::filesystem::path> paths{ct, mr};
    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE_FALSE(volume.has_value());
    CHECK(volume.error().code == ErrorCode::VolumeInconsistent);
}

TEST_CASE("fromSeries reports why nothing could be loaded", "[volume][error]")
{
    CHECK(dv::Volume::fromSeries({}).error().code == ErrorCode::NoDicomFiles);

    const std::vector<std::filesystem::path> missing{dataFile("missing.dcm")};
    CHECK(dv::Volume::fromSeries(missing).error().code == ErrorCode::FileNotFound);

    const TempDir dir;
    writeBytes(dir / "notes.txt", {});
    writeBytes(dir / "picture.png", {});
    const std::vector<std::filesystem::path> notDicom{dir / "notes.txt", dir / "picture.png"};
    CHECK(dv::Volume::fromSeries(notDicom).error().code == ErrorCode::NoDicomFiles);
}

TEST_CASE("fromSeries picks the most common series, not the first file's", "[volume]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{
        writeSlice(dir, "scout.dcm", {.z = 0.0, .seriesUid = "9.9.9", .marker = 99}),
        writeSlice(dir, "a.dcm", {.z = 0.0, .marker = 1}),
        writeSlice(dir, "b.dcm", {.z = 5.0, .marker = 2}),
        writeSlice(dir, "c.dcm", {.z = 10.0, .marker = 3}),
    };

    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE(volume.has_value());
    CHECK(volume->depth == 3);
    CHECK(firstVoxel(*volume, 0) == 1.0F - 1024.0F);
}

TEST_CASE("fromSeries falls back to the instance number when only some slices have a position", "[volume]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{
        writeSlice(dir, "a.dcm", {.z = 50.0, .instance = 2, .marker = 20}),
        writeSlice(dir, "b.dcm", {.instance = 1, .marker = 10}),
    };

    const auto volume = dv::Volume::fromSeries(paths);
    REQUIRE(volume.has_value());
    CHECK(firstVoxel(*volume, 0) == 10.0F - 1024.0F);
    CHECK(firstVoxel(*volume, 1) == 20.0F - 1024.0F);
}

TEST_CASE("loadSeries returns the slice files in volume order", "[volume]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{
        writeSlice(dir, "a.dcm", {.z = 10.0, .marker = 10}),
        writeSlice(dir, "b.dcm", {.z = 0.0, .marker = 0}),
        writeSlice(dir, "c.dcm", {.z = 5.0, .marker = 5}),
    };

    const auto series = dv::loadSeries(paths);
    REQUIRE(series.has_value());
    REQUIRE(series->slices.size() == 3);
    CHECK(series->slices[0].path().filename() == "b.dcm");
    CHECK(series->slices[1].path().filename() == "c.dcm");
    CHECK(series->slices[2].path().filename() == "a.dcm");
    CHECK(series->volume.depth == 3);
}

TEST_CASE("A single-slice series loads as a one-slice volume", "[volume]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{writeSlice(dir, "a.dcm", {.z = 0.0, .marker = 7})};

    const auto series = dv::loadSeries(paths);
    REQUIRE(series.has_value());
    CHECK(series->slices.size() == 1);
    CHECK(series->volume.depth == 1);
    CHECK(series->volume.spacing[2] == Catch::Approx(5.0)); // slice thickness
}

TEST_CASE("loadSeries stops when cancelled", "[volume][error]")
{
    const TempDir dir;
    const std::vector<std::filesystem::path> paths{writeSlice(dir, "a.dcm", {.z = 0.0}),
                                                   writeSlice(dir, "b.dcm", {.z = 5.0})};
    const std::atomic<bool> cancelled{true};
    const auto series = dv::loadSeries(paths, &cancelled);
    REQUIRE_FALSE(series.has_value());
    CHECK(series.error().detail == "cancelled");
}

TEST_CASE("fromSeries rejects multi-frame and color files", "[volume][error]")
{
    const std::vector<std::filesystem::path> color{dataFile("SC_rgb_small_odd.dcm")};
    CHECK(dv::Volume::fromSeries(color).error().code == ErrorCode::UnsupportedPhotometric);
}

TEST_CASE("fromMultiFrame stacks the frames in order", "[volume]")
{
    const TempDir dir;
    rewrite(dataFile("CT_small.dcm"), dir / "multi.dcm", [](DcmDataset& ds) {
        const Uint16* pixels = nullptr;
        unsigned long count = 0;
        REQUIRE(ds.findAndGetUint16Array(DCM_PixelData, pixels, &count).good());
        std::vector<Uint16> twoFrames(pixels, pixels + count);
        twoFrames.insert(twoFrames.end(), pixels, pixels + count);
        twoFrames[count] = 77; // first pixel of frame 1
        REQUIRE(ds.putAndInsertUint16Array(DCM_PixelData, twoFrames.data(), twoFrames.size()).good());
        ds.putAndInsertString(DCM_NumberOfFrames, "2");
    });

    const auto file = dv::DicomFile::load(dir / "multi.dcm");
    REQUIRE(file.has_value());
    REQUIRE(file->info().frameCount == 2);

    const auto volume = dv::Volume::fromMultiFrame(*file);
    REQUIRE(volume.has_value());
    CHECK(volume->depth == 2);
    CHECK(volume->voxels.size() == 128u * 128u * 2u);
    CHECK(firstVoxel(*volume, 1) == 77.0F - 1024.0F);
    CHECK(volume->spacing[2] == Catch::Approx(5.0));
    CHECK(volume->spacing[1] == Catch::Approx(0.661468));
}

TEST_CASE("fromMultiFrame of a single frame is a one-slice volume, color is rejected", "[volume][error]")
{
    const auto ct = dv::DicomFile::load(dataFile("CT_small.dcm"));
    REQUIRE(ct.has_value());
    CHECK(dv::Volume::fromMultiFrame(*ct)->depth == 1);

    const auto rgb = dv::DicomFile::load(dataFile("SC_rgb_rle_2frame.dcm"));
    REQUIRE(rgb.has_value());
    CHECK(dv::Volume::fromMultiFrame(*rgb).error().code == ErrorCode::UnsupportedPhotometric);
}
