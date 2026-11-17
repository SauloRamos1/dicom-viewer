// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <array>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include "TestSupport.h"
#include "Windowing.h"

using dv::Colormap;
using dv::ErrorCode;
using dv::Voi;
using dv::WindowPreset;
using namespace dv::test;

namespace {

dv::DicomFile loadOrFail(const std::filesystem::path& path)
{
    auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    return *std::move(file);
}

dv::RenderedImage render(const dv::DicomFile& file, Voi voi, bool invert = false, Colormap cmap = Colormap::Gray,
                         int frame = 0)
{
    auto image = dv::renderFrame(file, frame, {.voi = voi, .invert = invert, .colormap = cmap});
    REQUIRE(image.has_value());
    return *std::move(image);
}

} // namespace

TEST_CASE("Modality LUT values replace the stored values, in the window and the readout", "[windowing][lut]")
{
    const TempDir dir;
    writeModalityLutImage(dir / "lut.dcm", {0, 1, 0, 1}, {1000, 2000});
    const auto file = loadOrFail(dir / "lut.dcm");

    const auto values = file.rawModalityValues(0);
    REQUIRE(values.has_value());
    CHECK(*values == std::vector<float>{1000, 2000, 1000, 2000});
    CHECK(file.valueAt(1, 0, 0) == 2000.0F);

    // The default window spans the LUT output: one pixel black, the other white.
    const auto voi = dv::presetVoi(WindowPreset::MinMax, file);
    REQUIRE(voi.has_value());
    CHECK(voi->center == Catch::Approx(1500.5));
    CHECK(voi->width == Catch::Approx(1001.0));
    CHECK(render(file, *voi).data == std::vector<std::uint8_t>{0, 255, 0, 255});
}

TEST_CASE("Modality LUT values agree with DCMTK's own rendering", "[windowing][lut]")
{
    const auto isSigned = GENERATE(false, true);
    const TempDir dir;
    // Signed data maps -1..2 (first mapped value -1); unsigned maps 1..4.
    const int first = isSigned ? -1 : 1;
    writeModalityLutImage(dir / "lut.dcm",
                          {static_cast<std::uint16_t>(first), static_cast<std::uint16_t>(first + 1),
                           static_cast<std::uint16_t>(first + 2), static_cast<std::uint16_t>(first + 3),
                           0 /* below the first mapped value (or inside it, when signed) */},
                          {1000, 1400, 1700, 2000}, first, isSigned ? 1 : 0);
    const auto file = loadOrFail(dir / "lut.dcm");
    const auto values = file.rawModalityValues(0);
    REQUIRE(values.has_value());
    const Voi voi{1500, 800};
    const auto cpu = render(file, voi);
    REQUIRE(cpu.data.size() == values->size());
    for (std::size_t i = 0; i < values->size(); ++i) {
        CAPTURE(i, (*values)[i]);
        CHECK(std::abs(int{cpu.data[i]} - int{dv::applyWindow((*values)[i], voi)}) <= 1);
    }
}

TEST_CASE("a Modality LUT with fewer entries than its descriptor is corrupt", "[windowing][lut][error]")
{
    const TempDir dir;
    writeMonoImage(dir / "a.dcm", {.rows = 1, .columns = 2, .samples = {0, 1}});
    rewrite(dir / "a.dcm", dir / "b.dcm", [](DcmDataset& ds) {
        DcmItem* item = nullptr;
        REQUIRE(ds.findOrCreateSequenceItem(DCM_ModalityLUTSequence, item, 0).good());
        REQUIRE(item->putAndInsertUint16Array(DCM_LUTDescriptor, std::array<Uint16, 3>{4, 0, 16}.data(), 3).good());
        REQUIRE(item->putAndInsertUint16Array(DCM_LUTData, std::array<Uint16, 2>{10, 20}.data(), 2).good());
    });
    CHECK(loadOrFail(dir / "b.dcm").rawModalityValues(0).error().code == ErrorCode::CorruptPixelData);
}

TEST_CASE("MinMax of a 32-bit image comes from DCMTK instead of failing", "[windowing]")
{
    const TempDir dir;
    write32BitImage(dir / "wide.dcm", {0, 1000, 70000, 100000});
    const auto file = loadOrFail(dir / "wide.dcm");
    REQUIRE(file.rawModalityValues(0).has_value() == false); // the sample reader is 8/16-bit only

    const auto voi = dv::presetVoi(WindowPreset::MinMax, file);
    REQUIRE(voi.has_value());
    CHECK(voi->center == Catch::Approx(50000.5));
    CHECK(voi->width == Catch::Approx(100001.0));
}

TEST_CASE("applyWindow follows the DICOM linear function", "[windowing]")
{
    const Voi brain = dv::kCtBrain; // c=40 w=80: black <= 0, white > 79
    CHECK(dv::applyWindow(-1000, brain) == 0);
    CHECK(dv::applyWindow(0, brain) == 0);
    CHECK(dv::applyWindow(40, brain) == 129); // (0.5/79 + 0.5) * 255
    CHECK(dv::applyWindow(79, brain) == 255);
    CHECK(dv::applyWindow(80, brain) == 255);
    CHECK(dv::applyWindow(5000, brain) == 255);
}

TEST_CASE("applyWindow handles the exact window edges", "[windowing]")
{
    // Lower edge c - 0.5 - (w-1)/2 and upper edge c - 0.5 + (w-1)/2.
    const Voi voi{100, 11}; // edges at 94.5 and 104.5
    CHECK(dv::applyWindow(94.5, voi) == 0);
    CHECK(dv::applyWindow(94.6, voi) == 3); // (0.01) * 255 = 2.55
    CHECK(dv::applyWindow(104.5, voi) == 255);
    CHECK(dv::applyWindow(104.6, voi) == 255);
    CHECK(dv::applyWindow(99.5, voi) == 128); // centre
}

TEST_CASE("applyWindow treats widths below 1 as 1", "[windowing]")
{
    for (const double width : {1.0, 0.5, 0.0, -10.0}) {
        CAPTURE(width);
        const Voi voi{100, width}; // threshold at 99.5
        CHECK(dv::applyWindow(99, voi) == 0);
        CHECK(dv::applyWindow(99.5, voi) == 0);
        CHECK(dv::applyWindow(100, voi) == 255);
    }
}

TEST_CASE("applyWindow is monotonic and covers the full 8-bit range", "[windowing]")
{
    const Voi voi{-600, 1500};
    int previous = 0;
    for (double x = -2000; x <= 2000; x += 1.5) {
        const int y = dv::applyWindow(x, voi);
        REQUIRE(y >= previous);
        previous = y;
    }
    CHECK(dv::applyWindow(-2000, voi) == 0);
    CHECK(dv::applyWindow(2000, voi) == 255);
}

TEST_CASE("window presets", "[windowing]")
{
    const auto mr = loadOrFail(dataFile("MR_small.dcm"));

    CHECK(dv::presetVoi(WindowPreset::CtBrain, mr) == Voi{40, 80});
    CHECK(dv::presetVoi(WindowPreset::CtLung, mr) == Voi{-600, 1500});
    CHECK(dv::presetVoi(WindowPreset::CtBone, mr) == Voi{400, 1800});
    CHECK(dv::presetVoi(WindowPreset::CtAbdomen, mr) == Voi{40, 400});
    CHECK(dv::presetVoi(WindowPreset::FromFile, mr) == Voi{600, 1600});
}

TEST_CASE("MinMax preset spans the frame values, and is the fallback for FromFile", "[windowing]")
{
    const auto ct = loadOrFail(dataFile("CT_small.dcm"));
    const auto values = ct.rawModalityValues(0);
    REQUIRE(values.has_value());
    const auto [lo, hi] = std::ranges::minmax(*values);

    const auto minmax = dv::presetVoi(WindowPreset::MinMax, ct);
    REQUIRE(minmax.has_value());
    CHECK(minmax->width == Catch::Approx(hi - lo + 1));
    CHECK(minmax->center == Catch::Approx((hi + lo + 1) / 2.0));
    CHECK(dv::applyWindow(lo, *minmax) == 0);
    CHECK(dv::applyWindow(hi, *minmax) == 255);

    CHECK(*dv::presetVoi(WindowPreset::FromFile, ct) == *minmax); // CT_small has no window

    const auto rgb = loadOrFail(dataFile("SC_rgb_small_odd.dcm"));
    CHECK(dv::presetVoi(WindowPreset::MinMax, rgb).error().code == ErrorCode::UnsupportedPhotometric);
    CHECK(dv::presetVoi(WindowPreset::MinMax, ct, 3).error().code == ErrorCode::FrameOutOfRange);
}

TEST_CASE("renderFrame produces an 8-bit image matching applyWindow", "[windowing][render]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    const auto image = render(file, dv::kCtBone);
    CHECK(image.width == 128);
    CHECK(image.height == 128);
    CHECK(image.channels == 1);
    REQUIRE(image.data.size() == 128u * 128u);

    for (int y = 0; y < 128; y += 7) {
        for (int x = 0; x < 128; x += 5) {
            const double hu = *file.valueAt(x, y, 0);
            const int expected = dv::applyWindow(hu, dv::kCtBone);
            CHECK(std::abs(int{image.data[static_cast<std::size_t>(y * 128 + x)]} - expected) <= 1);
        }
    }
}

TEST_CASE("changing the window changes the output", "[windowing][render]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    const auto brain = render(file, dv::kCtBrain);
    const auto lung = render(file, dv::kCtLung);
    const auto brainAgain = render(file, dv::kCtBrain);

    CHECK(brain.data != lung.data);
    CHECK(brain.data == brainAgain.data);

    // A narrower window saturates more pixels.
    const auto saturated = [](const dv::RenderedImage& image) {
        return std::ranges::count_if(image.data, [](std::uint8_t v) { return v == 0 || v == 255; });
    };
    CHECK(saturated(brain) > saturated(lung));
}

TEST_CASE("a width below 1 renders as a binary threshold", "[windowing][render]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    const auto image = render(file, {.center = 0, .width = 0});
    CHECK(std::ranges::all_of(image.data, [](std::uint8_t v) { return v == 0 || v == 255; }));
}

TEST_CASE("invert flips the gray levels", "[windowing][render]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    const auto normal = render(file, dv::kCtBone);
    const auto inverted = render(file, dv::kCtBone, true);
    REQUIRE(normal.data.size() == inverted.data.size());
    for (std::size_t i = 0; i < normal.data.size(); ++i)
        REQUIRE(inverted.data[i] == 255 - normal.data[i]);
}

TEST_CASE("MONOCHROME1 renders inverted relative to MONOCHROME2", "[windowing][render]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "mono1.dcm",
            [](DcmDataset& ds) { REQUIRE(ds.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1").good()); });

    const auto mono2 = loadOrFail(dataFile("MR_small.dcm"));
    const auto mono1 = loadOrFail(dir / "mono1.dcm");
    REQUIRE(mono1.info().photometric == dv::Photometric::Monochrome1);
    REQUIRE(mono1.info().isMonochrome());

    const Voi voi = *mono2.info().defaultVoi;
    const auto a = render(mono2, voi);
    const auto b = render(mono1, voi);
    REQUIRE(a.data.size() == b.data.size());
    CHECK(a.data != b.data);
    for (std::size_t i = 0; i < a.data.size(); ++i)
        REQUIRE(std::abs(int{a.data[i]} + int{b.data[i]} - 255) <= 1);
}

TEST_CASE("colormap lookup tables have the expected endpoints", "[windowing][colormap]")
{
    using Rgb = std::array<std::uint8_t, 3>;
    for (std::size_t i = 0; i < 256; ++i) {
        const auto v = static_cast<std::uint8_t>(i);
        CHECK(dv::colormapLut(Colormap::Gray)[i] == Rgb{v, v, v});
    }
    CHECK(dv::colormapLut(Colormap::Bone)[0] == Rgb{0, 0, 0});
    CHECK(dv::colormapLut(Colormap::Bone)[255] == Rgb{255, 255, 255});
    CHECK(dv::colormapLut(Colormap::Hot)[0] == Rgb{0, 0, 0});
    CHECK(dv::colormapLut(Colormap::Hot)[255] == Rgb{255, 255, 255});
    CHECK(dv::colormapLut(Colormap::Jet)[0] == Rgb{0, 0, 128});
    CHECK(dv::colormapLut(Colormap::Jet)[255] == Rgb{128, 0, 0});

    // Hot: red saturates first, then green, then blue.
    const auto& hot = dv::colormapLut(Colormap::Hot);
    CHECK(hot[100][0] > hot[100][1]);
    CHECK(hot[100][1] >= hot[100][2]);
    // Jet: low = blue, middle = green, high = red.
    const auto& jet = dv::colormapLut(Colormap::Jet);
    CHECK(jet[20][2] > jet[20][0]);
    CHECK(jet[128][1] > jet[128][0]);
    CHECK(jet[235][0] > jet[235][2]);
}

TEST_CASE("a colormap makes the output 3-channel and maps each gray level", "[windowing][render][colormap]")
{
    const auto file = loadOrFail(dataFile("MR_small.dcm"));
    const Voi voi = *file.info().defaultVoi;
    const auto gray = render(file, voi);

    for (const Colormap cmap : {Colormap::Bone, Colormap::Hot, Colormap::Jet}) {
        const auto colored = render(file, voi, false, cmap);
        REQUIRE(colored.channels == 3);
        REQUIRE(colored.data.size() == gray.data.size() * 3);
        const auto& lut = dv::colormapLut(cmap);
        for (std::size_t i = 0; i < gray.data.size(); i += 11) {
            for (std::size_t c = 0; c < 3; ++c)
                REQUIRE(colored.data[i * 3 + c] == lut[gray.data[i]][c]);
        }
    }

    // Invert is applied before the colormap.
    const auto invertedBone = render(file, voi, true, Colormap::Bone);
    const auto& bone = dv::colormapLut(Colormap::Bone);
    for (std::size_t i = 0; i < gray.data.size(); i += 11)
        REQUIRE(invertedBone.data[i * 3] == bone[255 - gray.data[i]][0]);
}

TEST_CASE("RGB images render in color with the stored pixel values", "[windowing][render][rgb]")
{
    const auto file = loadOrFail(dataFile("SC_rgb_small_odd.dcm"));
    const auto bytes = readBytes(dataFile("SC_rgb_small_odd.dcm"));
    const std::size_t offset = pixelDataOffset(bytes);

    // Voi and colormap do not apply to color images.
    const auto image = render(file, dv::kCtBrain, false, Colormap::Jet);
    CHECK(image.channels == 3);
    CHECK(image.width == 3);
    CHECK(image.height == 3);
    REQUIRE(image.data.size() == 27);
    CHECK(std::equal(image.data.begin(), image.data.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset)));
    CHECK(std::vector<std::uint8_t>(image.data.begin(), image.data.begin() + 3)
          == std::vector<std::uint8_t>{0xA6, 0x8D, 0x34});

    const auto inverted = render(file, dv::kCtBrain, true);
    for (std::size_t i = 0; i < image.data.size(); ++i)
        REQUIRE(inverted.data[i] == 255 - image.data[i]);
}

TEST_CASE("compressed and multi-frame color images render", "[windowing][render][rgb]")
{
    const auto jpeg = loadOrFail(dataFile("SC_rgb_jpeg_dcmtk.dcm"));
    const auto jpegImage = render(jpeg, {});
    CHECK(jpegImage.channels == 3);
    CHECK(jpegImage.data.size() == 100u * 100u * 3u);

    const auto rle = loadOrFail(dataFile("SC_rgb_rle_2frame.dcm"));
    const auto first = render(rle, {}, false, Colormap::Gray, 0);
    const auto second = render(rle, {}, false, Colormap::Gray, 1);
    CHECK(first.data.size() == 100u * 100u * 3u);
    CHECK(first.data != second.data);
    CHECK(dv::renderFrame(rle, 2, {}).error().code == ErrorCode::FrameOutOfRange);
    CHECK(dv::renderFrame(rle, -1, {}).error().code == ErrorCode::FrameOutOfRange);

    const auto ybr = loadOrFail(dataFile("SC_ybr_full_422_uncompressed.dcm"));
    CHECK(render(ybr, {}).channels == 3);
}

TEST_CASE("every supported monochrome file renders at its own size", "[windowing][render]")
{
    const char* name = GENERATE("MR_small.dcm", "MR_small_bigendian.dcm", "MR_small_RLE.dcm",
                                "MR_small_jpeg_ls_lossless.dcm", "JPEG-lossy.dcm", "CT_small.dcm");
    CAPTURE(name);
    const auto file = loadOrFail(dataFile(name));
    const Voi voi = *dv::presetVoi(WindowPreset::FromFile, file);
    const auto image = render(file, voi);
    CHECK(image.width == file.info().columns);
    CHECK(image.height == file.info().rows);
    CHECK(image.data.size() == static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));
    const auto [lo, hi] = std::ranges::minmax(image.data);
    CHECK(lo != hi);
}

TEST_CASE("rendered output is identical across equivalent encodings", "[windowing][render][codecs]")
{
    const auto reference = render(loadOrFail(dataFile("MR_small.dcm")), dv::kCtBone);
    for (const char* name : {"MR_small_bigendian.dcm", "MR_small_RLE.dcm", "MR_small_jpeg_ls_lossless.dcm"}) {
        CAPTURE(name);
        CHECK(render(loadOrFail(dataFile(name)), dv::kCtBone).data == reference.data);
    }
}

TEST_CASE("renderFrame benchmarks", "[!benchmark]")
{
    const auto ct = loadOrFail(dataFile("CT_small.dcm"));
    const auto jpeg = loadOrFail(dataFile("JPEG-lossy.dcm"));
    BENCHMARK("renderFrame CT_small 128x128") { return dv::renderFrame(ct, 0, {.voi = dv::kCtBone}); };
    BENCHMARK("renderFrame JPEG-lossy 1024x256") { return dv::renderFrame(jpeg, 0, {.voi = {2000, 4000}}); };
}

TEST_CASE("value range follows the stored bits and the rescale", "[windowing][range]")
{
    dv::ImageInfo info;
    info.bitsStored = 12;
    info.pixelRepresentation = 0;
    info.rescaleSlope = 1;
    info.rescaleIntercept = -1024;
    auto range = dv::modalityRange(info);
    CHECK(range.min == -1024);
    CHECK(range.max == 3071);

    info.bitsStored = 16;
    info.pixelRepresentation = 1;
    info.rescaleIntercept = 0;
    range = dv::modalityRange(info);
    CHECK(range.min == -32768);
    CHECK(range.max == 32767);

    info.bitsStored = 8;
    info.pixelRepresentation = 0;
    info.rescaleSlope = 2;
    info.rescaleIntercept = 10;
    range = dv::modalityRange(info);
    CHECK(range.min == 10);
    CHECK(range.max == 520);
}

TEST_CASE("value range stays ordered for a negative slope and survives odd bits", "[windowing][range]")
{
    dv::ImageInfo info;
    info.bitsStored = 8;
    info.rescaleSlope = -1;
    info.rescaleIntercept = 255;
    auto range = dv::modalityRange(info);
    CHECK(range.min == 0);
    CHECK(range.max == 255);

    info.bitsStored = 0; // missing: fall back to the allocated bits
    info.bitsAllocated = 16;
    info.rescaleSlope = 1;
    info.rescaleIntercept = 0;
    range = dv::modalityRange(info);
    CHECK(range.min == 0);
    CHECK(range.max == 65535);
}

TEST_CASE("value range of a real file contains its window", "[windowing][range]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    const auto range = dv::modalityRange(file.info());
    REQUIRE(range.min < range.max);
    const auto values = file.rawModalityValues(0);
    REQUIRE(values.has_value());
    const auto [lo, hi] = std::ranges::minmax(*values);
    CHECK(range.min <= lo);
    CHECK(range.max >= hi);
}
