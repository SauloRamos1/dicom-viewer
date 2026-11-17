// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "DicomFile.h"
#include "TestSupport.h"

using dv::ErrorCode;
using dv::Photometric;
using namespace dv::test;

namespace {

struct Expected {
    const char* file;
    int rows;
    int columns;
    int bitsAllocated;
    Photometric photometric;
    int frames;
    const char* syntaxContains;
};

dv::DicomError loadError(const std::filesystem::path& path)
{
    const auto result = dv::DicomFile::load(path);
    REQUIRE_FALSE(result.has_value());
    return result.error();
}

// Layout attributes of a file built at runtime; `pixelBytes` bytes of pixel data are written,
// whatever the attributes claim.
struct Layout {
    int rows{1};
    int columns{1};
    const char* frames{"1"};
    int samplesPerPixel{1};
    const char* photometric{"MONOCHROME2"};
    int bitsAllocated{16};
    int bitsStored{16};
    std::size_t pixelBytes{2};
};

std::filesystem::path writeLayout(const TempDir& dir, const Layout& layout)
{
    const auto path = dir / "layout.dcm";
    rewrite(dataFile("MR_small.dcm"), path, [&](DcmDataset& ds) {
        ds.putAndInsertUint16(DCM_Rows, static_cast<Uint16>(layout.rows));
        ds.putAndInsertUint16(DCM_Columns, static_cast<Uint16>(layout.columns));
        ds.putAndInsertString(DCM_NumberOfFrames, layout.frames);
        ds.putAndInsertUint16(DCM_SamplesPerPixel, static_cast<Uint16>(layout.samplesPerPixel));
        ds.putAndInsertString(DCM_PhotometricInterpretation, layout.photometric);
        ds.putAndInsertUint16(DCM_BitsAllocated, static_cast<Uint16>(layout.bitsAllocated));
        ds.putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(layout.bitsStored));
        ds.putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(layout.bitsStored - 1));
        const std::vector<Uint8> bytes(layout.pixelBytes);
        ds.putAndInsertUint8Array(DCM_PixelData, bytes.data(), bytes.size());
    });
    return path;
}

} // namespace

TEST_CASE("load reads image attributes for every supported file", "[dicomfile]")
{
    const auto expected = GENERATE(
        Expected{"CT_small.dcm", 128, 128, 16, Photometric::Monochrome2, 1, "Little Endian Explicit"},
        Expected{"MR_small.dcm", 64, 64, 16, Photometric::Monochrome2, 1, "Little Endian Explicit"},
        Expected{"MR_small_bigendian.dcm", 64, 64, 16, Photometric::Monochrome2, 1, "Big Endian"},
        Expected{"MR_small_RLE.dcm", 64, 64, 16, Photometric::Monochrome2, 1, "RLE"},
        Expected{"MR_small_jpeg_ls_lossless.dcm", 64, 64, 16, Photometric::Monochrome2, 1, "JPEG-LS"},
        Expected{"JPEG-lossy.dcm", 1024, 256, 16, Photometric::Monochrome2, 1, "JPEG"},
        Expected{"SC_rgb_small_odd.dcm", 3, 3, 8, Photometric::Rgb, 1, "Little Endian"},
        Expected{"SC_rgb_jpeg_dcmtk.dcm", 100, 100, 8, Photometric::Rgb, 1, "JPEG"},
        Expected{"SC_rgb_rle_2frame.dcm", 100, 100, 8, Photometric::Rgb, 2, "RLE"},
        Expected{"SC_ybr_full_422_uncompressed.dcm", 100, 100, 8, Photometric::YbrFull422, 1, "Little Endian"});
    CAPTURE(expected.file);

    const auto file = dv::DicomFile::load(dataFile(expected.file));
    REQUIRE(file.has_value());
    const dv::ImageInfo& info = file->info();
    CHECK(info.rows == expected.rows);
    CHECK(info.columns == expected.columns);
    CHECK(info.bitsAllocated == expected.bitsAllocated);
    CHECK(info.photometric == expected.photometric);
    CHECK(info.frameCount == expected.frames);
    CHECK_THAT(info.transferSyntaxName, Catch::Matchers::ContainsSubstring(expected.syntaxContains));
    CHECK_FALSE(info.transferSyntaxUid.empty());
}

TEST_CASE("load exposes CT attributes", "[dicomfile]")
{
    const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
    REQUIRE(file.has_value());
    const dv::ImageInfo& info = file->info();
    CHECK(info.modality == "CT");
    CHECK(info.bitsStored == 16);
    CHECK(info.pixelRepresentation == 1);
    CHECK(info.rescaleSlope == 1.0);
    CHECK(info.rescaleIntercept == -1024.0);
    CHECK_FALSE(info.defaultVoi.has_value());
    REQUIRE(info.pixelSpacing.has_value());
    CHECK((*info.pixelSpacing)[0] == Catch::Approx(0.661468));
    CHECK(info.sliceThickness == 5.0);
    CHECK(info.imageOrientation.has_value());
    CHECK(info.imagePosition.has_value());
    CHECK(info.patientName == "CompressedSamples^CT1");
    CHECK(file->path() == dataFile("CT_small.dcm"));
}

TEST_CASE("load exposes the default window of MR files", "[dicomfile]")
{
    const auto file = dv::DicomFile::load(dataFile("MR_small.dcm"));
    REQUIRE(file.has_value());
    CHECK(file->info().defaultVoi == dv::Voi{600, 1600});
    CHECK(file->info().isMonochrome());
}

TEST_CASE("compressed files decode to the same pixels as uncompressed ones", "[dicomfile][codecs]")
{
    const auto reference = dv::DicomFile::load(dataFile("MR_small.dcm"));
    REQUIRE(reference.has_value());
    const auto expected = reference->rawModalityValues(0);
    REQUIRE(expected.has_value());

    const char* name = GENERATE("MR_small_bigendian.dcm", "MR_small_RLE.dcm", "MR_small_jpeg_ls_lossless.dcm");
    CAPTURE(name);
    const auto file = dv::DicomFile::load(dataFile(name));
    REQUIRE(file.has_value());
    const auto values = file->rawModalityValues(0);
    REQUIRE(values.has_value());
    CHECK(*values == *expected);
}

TEST_CASE("load reports FileNotFound for a missing path", "[dicomfile][error]")
{
    const auto error = loadError(dataFile("does_not_exist.dcm"));
    CHECK(error.code == ErrorCode::FileNotFound);
    CHECK_FALSE(error.message.empty());
}

TEST_CASE("load reports FileUnreadable for a directory", "[dicomfile][error]")
{
    const TempDir dir;
    CHECK(loadError(dir.path()).code == ErrorCode::FileUnreadable);
}

TEST_CASE("load reports NotDicom for non-DICOM content", "[dicomfile][error]")
{
    const TempDir dir;

    SECTION("empty file")
    {
        writeBytes(dir / "empty.dcm", {});
        CHECK(loadError(dir / "empty.dcm").code == ErrorCode::NotDicom);
    }
    SECTION("random bytes")
    {
        std::mt19937 rng(12345);
        std::vector<std::uint8_t> bytes(4096);
        for (auto& b : bytes)
            b = static_cast<std::uint8_t>(rng());
        writeBytes(dir / "random.dcm", bytes);
        CHECK(loadError(dir / "random.dcm").code == ErrorCode::NotDicom);
    }
    SECTION("plain text")
    {
        const std::string text = "this is not a DICOM file";
        writeBytes(dir / "text.dcm", {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
        CHECK(loadError(dir / "text.dcm").code == ErrorCode::NotDicom);
    }
    SECTION("truncated after the preamble and header start")
    {
        auto bytes = readBytes(dataFile("MR_small.dcm"));
        bytes.resize(200);
        writeBytes(dir / "truncated.dcm", bytes);
        const auto error = loadError(dir / "truncated.dcm");
        CHECK(error.code == ErrorCode::NotDicom);
        CHECK_FALSE(error.detail.empty());
    }
}

TEST_CASE("load reports corrupt pixel data when the pixel value is cut short", "[dicomfile][error]")
{
    const TempDir dir;
    auto bytes = readBytes(dataFile("MR_small.dcm"));
    bytes.resize(bytes.size() - 2000);
    writeBytes(dir / "short.dcm", bytes);
    const auto code = loadError(dir / "short.dcm").code;
    CHECK((code == ErrorCode::CorruptPixelData || code == ErrorCode::NotDicom));
}

TEST_CASE("load reports NoPixelData when the pixel data element is absent", "[dicomfile][error]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "nopixels.dcm",
            [](DcmDataset& ds) { ds.findAndDeleteElement(DCM_PixelData); });
    const auto error = loadError(dir / "nopixels.dcm");
    CHECK(error.code == ErrorCode::NoPixelData);
    CHECK(error.tag == "(7FE0,0010)");
}

TEST_CASE("load reports unsupported transfer syntaxes with their name", "[dicomfile][error]")
{
    const auto error = loadError(dataFile("JPEG2000.dcm"));
    CHECK(error.code == ErrorCode::UnsupportedTransferSyntax);
    CHECK_THAT(error.detail, Catch::Matchers::ContainsSubstring("JPEG 2000"));

    const auto deflated = loadError(dataFile("image_dfl.dcm"));
    CHECK(deflated.code == ErrorCode::UnsupportedTransferSyntax);
    CHECK_THAT(deflated.detail, Catch::Matchers::ContainsSubstring("Deflated"));
}

TEST_CASE("load reports corrupt pixel data for invalid image attributes", "[dicomfile][error]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "norows.dcm", [](DcmDataset& ds) { ds.findAndDeleteElement(DCM_Rows); });
    CHECK(loadError(dir / "norows.dcm").code == ErrorCode::CorruptPixelData);
}

TEST_CASE("DicomFile copies share the decoded state", "[dicomfile]")
{
    const auto file = dv::DicomFile::load(dataFile("MR_small_RLE.dcm"));
    REQUIRE(file.has_value());
    const dv::DicomFile copy = *file;
    CHECK(*copy.rawModalityValues(0) == *file->rawModalityValues(0));
}

TEST_CASE("load rejects BitsAllocated values the viewer cannot decode before touching pixels", "[dicomfile][error]")
{
    const TempDir dir;
    // 64 allocated bits with 33 stored used to reach `1u << 33` in the sample reader.
    for (const Layout layout : {Layout{.bitsAllocated = 64, .bitsStored = 33, .pixelBytes = 8},
                                Layout{.bitsAllocated = 12, .bitsStored = 12, .pixelBytes = 2}}) {
        CAPTURE(layout.bitsAllocated);
        const auto error = loadError(writeLayout(dir, layout));
        CHECK(error.code == ErrorCode::UnsupportedPixelFormat);
        CHECK_THAT(error.detail, Catch::Matchers::ContainsSubstring("BitsAllocated " + std::to_string(layout.bitsAllocated)));
    }
}

TEST_CASE("a 1-bit image is unsupported, not truncated", "[dicomfile][error]")
{
    const TempDir dir;
    // 1x16 pixels of one bit need 2 bytes: the file is complete, only the format is out of scope.
    const auto error = loadError(writeLayout(dir, {.columns = 16, .bitsAllocated = 1, .bitsStored = 1, .pixelBytes = 2}));
    CHECK(error.code == ErrorCode::UnsupportedPixelFormat);
    CHECK_THAT(error.detail, Catch::Matchers::ContainsSubstring("BitsAllocated 1 is not supported"));
}

TEST_CASE("BitsStored beyond BitsAllocated is corrupt", "[dicomfile][error]")
{
    const TempDir dir;
    CHECK(loadError(writeLayout(dir, {.bitsAllocated = 16, .bitsStored = 17, .pixelBytes = 2})).code
          == ErrorCode::CorruptPixelData);
}

TEST_CASE("an image size that overflows size_t is rejected", "[dicomfile][error]")
{
    const TempDir dir;
    // 32768 * 32768 * 2^30 frames * 4 samples * 4 bytes = 2^64, which wraps to 0 bytes expected.
    const auto error = loadError(writeLayout(dir, {.rows = 32768, .columns = 32768, .frames = "1073741824",
                                                   .samplesPerPixel = 4, .bitsAllocated = 32, .bitsStored = 32,
                                                   .pixelBytes = 4}));
    CHECK(error.code == ErrorCode::CorruptPixelData);
}

TEST_CASE("a header claiming a huge image over a short pixel element is corrupt", "[dicomfile][error]")
{
    const TempDir dir;
    CHECK(loadError(writeLayout(dir, {.rows = 65535, .columns = 65535, .frames = "2147483647"})).code
          == ErrorCode::CorruptPixelData);
}

TEST_CASE("32-bit monochrome images still load (CPU rendering only)", "[dicomfile]")
{
    const TempDir dir;
    const auto file = dv::DicomFile::load(writeLayout(dir, {.bitsAllocated = 32, .bitsStored = 32, .pixelBytes = 4}));
    REQUIRE(file.has_value());
    CHECK(file->rawModalityValues(0).error().code == ErrorCode::CorruptPixelData);
}
