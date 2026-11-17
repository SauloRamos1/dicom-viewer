#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include "GpuTexture.h"
#include "ShaderReference.h"
#include "TestSupport.h"
#include "Windowing.h"

using dv::Colormap;
using dv::ErrorCode;
using dv::Voi;
using namespace dv::test;

namespace {

dv::DicomFile loadOrFail(const std::filesystem::path& path)
{
    auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    return *std::move(file);
}

dv::PackedFrame packOrFail(const dv::DicomFile& file, int frame = 0)
{
    auto packed = dv::packFrame(file, frame);
    REQUIRE(packed.has_value());
    return *std::move(packed);
}

// Stored sample patterns covering 0, 1, the middle and the extremes of `bits`
// (for signed data the extremes are the most negative and most positive values).
std::vector<std::uint16_t> extremes(int bits, bool isSigned)
{
    const std::uint32_t top = (1U << bits) - 1U;
    std::vector<std::uint16_t> samples{0, 1, 2, static_cast<std::uint16_t>(top / 2), static_cast<std::uint16_t>(top / 2 + 1),
                                       static_cast<std::uint16_t>(top - 1), static_cast<std::uint16_t>(top)};
    if (isSigned) // sign-extend patterns the way a 16-bit container stores them
        for (auto& s : samples)
            if ((s >> (bits - 1)) & 1U)
                s = static_cast<std::uint16_t>(s | ~top);
    return samples;
}

} // namespace

TEST_CASE("packFrame stores the stored value as high byte, low byte", "[gpu][pack]")
{
    const TempDir dir;
    writeMonoImage(dir / "a.dcm", {.rows = 1, .columns = 3, .samples = {0x0000, 0x1234, 0xFFFF}});
    const auto file = loadOrFail(dir / "a.dcm");

    const auto packed = packOrFail(file);
    CHECK(packed.width == 3);
    CHECK(packed.height == 1);
    REQUIRE(packed.rgba.size() == 12);
    CHECK(packed.rgba == std::vector<std::uint8_t>{0x00, 0x00, 0, 255, 0x12, 0x34, 0, 255, 0xFF, 0xFF, 0, 255});
}

TEST_CASE("the unpack formula returns the stored value for 8..16 bit data", "[gpu][pack]")
{
    struct Case {
        int allocated, stored, pixelRepresentation;
    };
    const auto c = GENERATE(Case{8, 8, 0}, Case{8, 7, 0}, Case{16, 12, 0}, Case{16, 16, 0}, Case{16, 16, 1}, Case{16, 12, 1});
    CAPTURE(c.allocated, c.stored, c.pixelRepresentation);

    const bool isSigned = c.pixelRepresentation == 1;
    const auto samples = extremes(c.stored, isSigned);
    const TempDir dir;
    writeMonoImage(dir / "a.dcm", {.rows = 1,
                                   .columns = static_cast<int>(samples.size()),
                                   .bitsAllocated = c.allocated,
                                   .bitsStored = c.stored,
                                   .pixelRepresentation = c.pixelRepresentation,
                                   .samples = samples});
    const auto file = loadOrFail(dir / "a.dcm");
    REQUIRE(dv::gpuSupported(file));
    const auto params = dv::gpuParams(file.info());
    CHECK(params.storedMin == (isSigned ? -std::ldexp(1.0, c.stored - 1) : 0.0));

    const auto packed = packOrFail(file);
    const auto values = file.rawModalityValues(0);
    REQUIRE(values.has_value());
    REQUIRE(packed.rgba.size() == samples.size() * 4);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        CAPTURE(i, samples[i]);
        const float u = shaderUnpack(packed.rgba[i * 4], packed.rgba[i * 4 + 1]);
        CHECK(u >= 0.0F);
        CHECK(u <= static_cast<float>((1U << c.stored) - 1U));
        // Identity rescale: the shader value is the stored value, as for rawModalityValues.
        CHECK(shaderValue(packed.rgba[i * 4], packed.rgba[i * 4 + 1], params) == (*values)[i]);
        CHECK(packed.rgba[i * 4 + 2] == 0);
        CHECK(packed.rgba[i * 4 + 3] == 255);
    }
}

TEST_CASE("slope and intercept are applied like rawModalityValues", "[gpu][pack]")
{
    const auto isSigned = GENERATE(false, true);
    const auto samples = extremes(16, isSigned);
    const TempDir dir;
    writeMonoImage(dir / "a.dcm", {.rows = 1,
                                   .columns = static_cast<int>(samples.size()),
                                   .pixelRepresentation = isSigned ? 1 : 0,
                                   .slope = 0.25,
                                   .intercept = -1024.0,
                                   .samples = samples});
    const auto file = loadOrFail(dir / "a.dcm");
    const auto params = dv::gpuParams(file.info());
    CHECK(params.slope == 0.25);
    CHECK(params.intercept == -1024.0);

    const auto packed = packOrFail(file);
    const auto values = file.rawModalityValues(0);
    REQUIRE(values.has_value());
    for (std::size_t i = 0; i < samples.size(); ++i)
        CHECK(shaderValue(packed.rgba[i * 4], packed.rgba[i * 4 + 1], params) == (*values)[i]);
}

TEST_CASE("packFrame handles multi-frame offsets and rejects bad frames", "[gpu][pack][error]")
{
    const auto file = loadOrFail(dataFile("CT_small.dcm"));
    CHECK(dv::packFrame(file, 1).error().code == ErrorCode::FrameOutOfRange);
    CHECK(dv::packFrame(file, -1).error().code == ErrorCode::FrameOutOfRange);
}

TEST_CASE("only 8 and 16 bit single-sample monochrome images are GPU capable", "[gpu][pack]")
{
    CHECK(dv::gpuSupported(loadOrFail(dataFile("CT_small.dcm"))));
    CHECK(dv::gpuSupported(loadOrFail(dataFile("MR_small.dcm"))));

    const auto rgb = loadOrFail(dataFile("SC_rgb_small_odd.dcm"));
    CHECK_FALSE(dv::gpuSupported(rgb));
    CHECK(dv::packFrame(rgb, 0).error().code == ErrorCode::UnsupportedPhotometric);

    const TempDir dir;
    rewrite(dataFile("CT_small.dcm"), dir / "lut.dcm", [](DcmDataset& ds) {
        DcmItem* item = nullptr;
        REQUIRE(ds.findOrCreateSequenceItem(DCM_ModalityLUTSequence, item, 0).good());
        REQUIRE(item->putAndInsertUint16Array(DCM_LUTDescriptor, std::array<Uint16, 3>{2, 0, 16}.data(), 3).good());
        REQUIRE(item->putAndInsertUint16Array(DCM_LUTData, std::array<Uint16, 2>{0, 100}.data(), 2).good());
    });
    const auto withLut = loadOrFail(dir / "lut.dcm");
    CHECK_FALSE(dv::gpuSupported(withLut)); // only DicomImage applies a Modality LUT
    CHECK(dv::packFrame(withLut, 0).error().code == ErrorCode::UnsupportedPhotometric);
}

TEST_CASE("MONOCHROME1 is flagged for the shader", "[gpu][pack]")
{
    const TempDir dir;
    writeMonoImage(dir / "m1.dcm", {.rows = 1, .columns = 2, .photometric = "MONOCHROME1", .samples = {0, 1}});
    CHECK(dv::gpuParams(loadOrFail(dir / "m1.dcm").info()).monochrome1);
    CHECK_FALSE(dv::gpuParams(loadOrFail(dataFile("MR_small.dcm")).info()).monochrome1);
}

// Gray level parity between DCMTK (CPU path) and the float32 shader math. They may
// differ by one level: both truncate the scaled value, but DCMTK evaluates it in
// double and the shader in float, so a pixel whose value lies within float
// rounding of an integer boundary can truncate the other way.
TEST_CASE("the shader math matches the CPU renderer within one gray level", "[gpu][parity]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "mono1.dcm", [](DcmDataset& ds) {
        REQUIRE(ds.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1").good());
    });

    struct Source {
        const char* name;
        std::filesystem::path path;
    };
    const std::array sources{Source{"CT_small", dataFile("CT_small.dcm")}, Source{"MR_small", dataFile("MR_small.dcm")},
                             Source{"MONOCHROME1", dir / "mono1.dcm"}};
    const Source& source = sources[static_cast<std::size_t>(GENERATE(0, 1, 2))];
    const auto file = loadOrFail(source.path);
    const auto packed = packOrFail(file);
    const auto params = dv::gpuParams(file.info());
    const auto fileVoi = *dv::presetVoi(dv::WindowPreset::FromFile, file);

    struct WindowCase {
        const char* name;
        Voi voi;
        bool invert;
    };
    const auto cases = {WindowCase{"default", fileVoi, false}, WindowCase{"narrow", {fileVoi.center, 20}, false},
                        WindowCase{"wide", {fileVoi.center, 100000}, false}, WindowCase{"inverted", fileVoi, true},
                        WindowCase{"threshold", {fileVoi.center, 1}, false},
                        WindowCase{"brain", dv::kCtBrain, false}, WindowCase{"lung", dv::kCtLung, true}};
    for (const auto& w : cases) {
        CAPTURE(source.name, w.name);
        const auto cpu = dv::renderFrame(file, 0, {.voi = w.voi, .invert = w.invert});
        REQUIRE(cpu.has_value());
        const auto gpu = shaderRender(packed, params, {.voi = w.voi, .invert = w.invert});
        REQUIRE(gpu.size() == cpu->data.size() * 3);

        int maxDiff = 0;
        std::size_t differing = 0;
        for (std::size_t i = 0; i < cpu->data.size(); ++i) {
            REQUIRE(gpu[i * 3] == gpu[i * 3 + 1]);
            REQUIRE(gpu[i * 3] == gpu[i * 3 + 2]);
            const int d = std::abs(int{gpu[i * 3]} - int{cpu->data[i]});
            maxDiff = std::max(maxDiff, d);
            differing += d != 0 ? 1 : 0;
        }
        CAPTURE(maxDiff, differing);
        CHECK(maxDiff <= 1);
        CHECK(differing * 100 <= cpu->data.size()); // at most 1 % of the pixels (observed: under 0.3 %)
    }
}

TEST_CASE("the shader colormap lookup matches the CPU colormapped image", "[gpu][parity][colormap]")
{
    const auto file = loadOrFail(dataFile("MR_small.dcm"));
    const auto packed = packOrFail(file);
    const auto params = dv::gpuParams(file.info());
    const Voi voi = *file.info().defaultVoi;
    const bool invert = GENERATE(false, true);

    const auto gray = dv::renderFrame(file, 0, {.voi = voi, .invert = invert});
    REQUIRE(gray.has_value());
    for (const Colormap cmap : {Colormap::Gray, Colormap::Bone, Colormap::Hot, Colormap::Jet}) {
        const auto cpu = dv::renderFrame(file, 0, {.voi = voi, .invert = invert, .colormap = cmap});
        REQUIRE(cpu.has_value());
        const auto gpu = shaderRender(packed, params, {.voi = voi, .invert = invert, .colormap = cmap});
        const int channels = cpu->channels;
        for (std::size_t i = 0; i < gray->data.size(); ++i) {
            // Where both paths pick the same gray level they pick the same color.
            const auto shaderGrayLevel = gpu[i * 3] == gpu[i * 3 + 1] && gpu[i * 3] == gpu[i * 3 + 2];
            if (cmap == Colormap::Gray)
                REQUIRE(shaderGrayLevel);
            const auto expected = dv::colormapLut(cmap)[gray->data[i]];
            const std::array<std::uint8_t, 3> got{gpu[i * 3], gpu[i * 3 + 1], gpu[i * 3 + 2]};
            const std::array<std::uint8_t, 3> cpuRgb = channels == 3
                ? std::array<std::uint8_t, 3>{cpu->data[i * 3], cpu->data[i * 3 + 1], cpu->data[i * 3 + 2]}
                : std::array<std::uint8_t, 3>{cpu->data[i], cpu->data[i], cpu->data[i]};
            REQUIRE(cpuRgb == expected);
            if (got != expected) { // the one-level rounding difference: must still be a neighbouring LUT entry
                const auto& lut = dv::colormapLut(cmap);
                const auto near = [&](int level) {
                    return got == lut[static_cast<std::size_t>(std::clamp(level, 0, 255))];
                };
                REQUIRE((near(gray->data[i] - 1) || near(gray->data[i] + 1)));
            }
        }
    }
}

// Run with: dicomcore_tests "[!benchmark]"   (hidden from the default run)
TEST_CASE("packing a 2048x2048 16-bit frame vs rendering it on the CPU", "[!benchmark][gpu][bench]")
{
    constexpr int kSize = 2048;
    std::vector<std::uint16_t> samples(static_cast<std::size_t>(kSize) * kSize);
    std::uint32_t seed = 12345;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        seed = seed * 1664525U + 1013904223U;
        const auto x = static_cast<std::uint32_t>(i % kSize);
        samples[i] = static_cast<std::uint16_t>((x * 16U + (seed >> 24)) & 0xFFFFU);
    }
    static const TempDir dir;
    writeMonoImage(dir / "large.dcm", {.rows = kSize, .columns = kSize, .samples = std::move(samples)});
    const auto file = loadOrFail(dir / "large.dcm");
    const Voi voi{16384, 8192};

    BENCHMARK("CPU renderFrame (DicomImage window + 8-bit output)") { return dv::renderFrame(file, 0, {.voi = voi}); };
    BENCHMARK("CPU renderFrame with Jet colormap") { return dv::renderFrame(file, 0, {.voi = voi, .colormap = Colormap::Jet}); };
    BENCHMARK("GPU upload prep: packFrame (once per frame, not per window change)") { return dv::packFrame(file, 0); };
}
