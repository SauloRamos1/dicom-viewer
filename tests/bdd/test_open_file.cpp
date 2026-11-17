// SPDX-License-Identifier: GPL-3.0-only
// Step definitions for tests/features/open_file.feature (core-level scenarios).
// The viewer-level ones (DicomController, LanguageManager) are in test_open_file_app.cpp.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "DicomFile.h"
#include "TestSupport.h"
#include "Windowing.h"

using namespace dv::test;

SCENARIO("Open a valid monochrome file", "[bdd][open_file]")
{
    GIVEN("the viewer is showing the empty state")
    {
        std::optional<dv::DicomFile> shown;

        WHEN("I open a valid monochrome DICOM file")
        {
            auto loaded = dv::DicomFile::load(dataFile("CT_small.dcm"));
            REQUIRE(loaded.has_value());
            shown = *std::move(loaded);
            const auto voi = dv::presetVoi(dv::WindowPreset::FromFile, *shown);
            REQUIRE(voi.has_value());
            const auto image = dv::renderFrame(*shown, 0, {.voi = *voi});
            REQUIRE(image.has_value());

            THEN("the image is shown with the dimensions stored in the file")
            {
                CHECK(image->width == 128);
                CHECK(image->height == 128);
                CHECK(image->channels == 1);
                CHECK(image->width == shown->info().columns);
                CHECK(image->height == shown->info().rows);
            }
        }
    }
}

SCENARIO("Open an invalid file reports a typed error", "[bdd][open_file]")
{
    GIVEN("a file that is not a valid DICOM file")
    {
        const TempDir dir;
        writeBytes(dir / "broken.dcm", {});

        WHEN("I open it")
        {
            const auto result = dv::DicomFile::load(dir / "broken.dcm");

            THEN("loading fails with an error code and a message")
            {
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error().code == dv::ErrorCode::NotDicom);
                CHECK_FALSE(result.error().message.empty());
                CHECK(result.error().severity == dv::Severity::Error);
            }
        }
    }
}

SCENARIO("Open a DICOM file without pixel data", "[bdd][open_file]")
{
    GIVEN("a valid DICOM file without pixel data")
    {
        const TempDir dir;
        rewrite(dataFile("MR_small.dcm"), dir / "nopixels.dcm", [](DcmDataset& ds) { ds.findAndDeleteElement(DCM_PixelData); });

        WHEN("I open it")
        {
            const auto result = dv::DicomFile::load(dir / "nopixels.dcm");

            THEN("the error names the missing pixel data tag")
            {
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error().code == dv::ErrorCode::NoPixelData);
                CHECK(result.error().tag == "(7FE0,0010)");
            }
        }
    }
}

SCENARIO("Change window level and width", "[bdd][open_file]")
{
    GIVEN("a valid monochrome DICOM file is open")
    {
        const auto file = dv::DicomFile::load(dataFile("CT_small.dcm"));
        REQUIRE(file.has_value());
        const auto before = dv::renderFrame(*file, 0, {.voi = dv::kCtBrain});
        REQUIRE(before.has_value());

        WHEN("I change the window level or width")
        {
            const auto after = dv::renderFrame(*file, 0, {.voi = dv::kCtLung});
            REQUIRE(after.has_value());

            THEN("the displayed pixel values change as expected")
            {
                CHECK(after->data != before->data);
                // The narrow brain window saturates more pixels than the wide lung window.
                const auto saturated = [](const dv::RenderedImage& image) {
                    return std::ranges::count_if(image.data, [](std::uint8_t v) { return v == 0 || v == 255; });
                };
                CHECK(saturated(*before) > saturated(*after));
            }
        }
    }
}

SCENARIO("Open an RGB file", "[bdd][open_file]")
{
    WHEN("I open a valid RGB DICOM file")
    {
        const auto file = dv::DicomFile::load(dataFile("SC_rgb_small_odd.dcm"));
        REQUIRE(file.has_value());
        const auto image = dv::renderFrame(*file, 0, {});
        REQUIRE(image.has_value());

        THEN("the image is displayed in color")
        {
            CHECK(image->channels == 3);
            CHECK(file->info().photometric == dv::Photometric::Rgb);
            // Not gray: the first pixel has distinct R, G and B components.
            CHECK((image->data[0] != image->data[1] || image->data[1] != image->data[2]));
        }
    }
}
