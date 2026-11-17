// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <random>
#include <string>

#include <dcmtk/dcmdata/dcdeftag.h>

#include "DicomFile.h"
#include "Metadata.h"
#include "Text.h"
#include "TestSupport.h"

using namespace dv::test;

namespace {

constexpr const char* kJoseLatin1 = "Jos\xE9^Mu\xF1oz";
constexpr const char* kJoseUtf8 = "Jos\xC3\xA9^Mu\xC3\xB1oz";

// MR_small.dcm with the given Specific Character Set (none when null) and Patient Name bytes.
std::filesystem::path writeNamed(const TempDir& dir, const char* charset, const char* name)
{
    const auto path = dir / "named.dcm";
    rewrite(dataFile("MR_small.dcm"), path, [&](DcmDataset& ds) {
        if (charset != nullptr)
            ds.putAndInsertString(DCM_SpecificCharacterSet, charset);
        else
            ds.findAndDeleteElement(DCM_SpecificCharacterSet);
        ds.putAndInsertString(DCM_PatientName, name);
        ds.putAndInsertString(DCM_PatientID, name);
        ds.putAndInsertString(DCM_StudyDescription, name);
        ds.putAndInsertString(DCM_SeriesDescription, name);
    });
    return path;
}

std::string metadataValue(const std::filesystem::path& path, std::uint16_t group, std::uint16_t element)
{
    const auto entries = dv::readMetadata(path);
    REQUIRE(entries.has_value());
    for (const auto& entry : *entries) {
        if (entry.group == group && entry.element == element)
            return entry.value;
    }
    FAIL("tag not found");
    return {};
}

} // namespace

TEST_CASE("ISO_IR 100 text is converted from Latin-1", "[text]")
{
    CHECK(dv::toUtf8(kJoseLatin1, "ISO_IR 100") == kJoseUtf8);
    CHECK(dv::toUtf8(kJoseLatin1, "ISO 2022 IR 100") == kJoseUtf8);
    CHECK(dv::toUtf8(kJoseLatin1, " iso_ir 100 ") == kJoseUtf8);
    CHECK(dv::toUtf8("plain", "ISO_IR 100") == "plain");
}

TEST_CASE("UTF-8, ASCII and unset character sets keep valid text", "[text]")
{
    CHECK(dv::toUtf8(kJoseUtf8, "ISO_IR 192") == kJoseUtf8);
    CHECK(dv::toUtf8(kJoseUtf8, "") == kJoseUtf8);
    CHECK(dv::toUtf8("ASCII only", "ISO_IR 6") == "ASCII only");
    CHECK(dv::toUtf8("", "ISO_IR 192").empty());
}

TEST_CASE("text that is not valid in its declared set falls back to Latin-1", "[text]")
{
    for (const char* charset : {"ISO_IR 192", "", "ISO_IR 6", "ISO_IR 144", "GB18030", "ISO 2022 IR 87"}) {
        CAPTURE(charset);
        const std::string out = dv::toUtf8(kJoseLatin1, charset);
        CHECK(dv::isValidUtf8(out));
        CHECK(out == kJoseUtf8);
    }
    CHECK(dv::toUtf8(kJoseUtf8, "ISO_IR 144") == kJoseUtf8); // valid UTF-8 is kept for sets without a table
}

TEST_CASE("only the first value of a multi-valued character set is used", "[text]")
{
    CHECK(dv::toUtf8(kJoseLatin1, "ISO_IR 100\\ISO 2022 IR 87") == kJoseUtf8);
    CHECK(dv::toUtf8(kJoseLatin1, "\\ISO 2022 IR 100") == kJoseUtf8); // default repertoire first: fallback applies
    CHECK(dv::toUtf8(kJoseUtf8, "ISO 2022 IR 6\\ISO 2022 IR 100") == kJoseUtf8);
}

TEST_CASE("toUtf8 always returns valid UTF-8, whatever the bytes", "[text]")
{
    std::mt19937 rng(7);
    for (const char* charset : {"", "ISO_IR 100", "ISO_IR 192", "ISO_IR 13", "nonsense"}) {
        for (int round = 0; round < 200; ++round) {
            std::string bytes(static_cast<std::size_t>(rng() % 24), '\0');
            for (char& c : bytes)
                c = static_cast<char>(rng() & 0xFF);
            CAPTURE(charset, round);
            CHECK(dv::isValidUtf8(dv::toUtf8(bytes, charset)));
        }
    }
}

TEST_CASE("isValidUtf8 rejects overlong, truncated, surrogate and out-of-range sequences", "[text]")
{
    CHECK(dv::isValidUtf8(""));
    CHECK(dv::isValidUtf8("\xF0\x9F\x98\x80")); // U+1F600
    for (const char* bad : {"\xC0\x80", "\xC3", "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\x80", "\xFF", "a\xE9"})
        CHECK_FALSE(dv::isValidUtf8(bad));
}

TEST_CASE("patient and study text of a Latin-1 file is read as UTF-8", "[text][dicomfile]")
{
    const TempDir dir;
    const auto file = dv::DicomFile::load(writeNamed(dir, "ISO_IR 100", kJoseLatin1));
    REQUIRE(file.has_value());
    CHECK(file->info().patientName == kJoseUtf8);
    CHECK(file->info().patientId == kJoseUtf8);
    CHECK(file->info().studyDescription == kJoseUtf8);
    CHECK(file->info().seriesDescription == kJoseUtf8);
}

TEST_CASE("the metadata table shows Latin-1 values as UTF-8, by path and by file", "[text][metadata]")
{
    const TempDir dir;
    const auto path = writeNamed(dir, "ISO_IR 100", kJoseLatin1);
    CHECK(metadataValue(path, 0x0010, 0x0010) == kJoseUtf8);
    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto entries = dv::readMetadata(*file);
    REQUIRE(entries.has_value());
    for (const auto& entry : *entries) {
        if (entry.group == 0x0010 && entry.element == 0x0010)
            CHECK(entry.value == kJoseUtf8);
    }
}

TEST_CASE("UTF-8 files pass through and files without a character set never yield invalid UTF-8", "[text][dicomfile]")
{
    const TempDir dir;
    const auto utf8 = dv::DicomFile::load(writeNamed(dir, "ISO_IR 192", kJoseUtf8));
    REQUIRE(utf8.has_value());
    CHECK(utf8->info().patientName == kJoseUtf8);

    const auto none = dv::DicomFile::load(writeNamed(dir, nullptr, kJoseLatin1));
    REQUIRE(none.has_value());
    CHECK(none->info().patientName == kJoseUtf8);
    CHECK(dv::isValidUtf8(metadataValue(dir / "named.dcm", 0x0010, 0x0010)));
}
