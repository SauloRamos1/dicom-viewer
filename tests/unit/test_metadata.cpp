// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include "Metadata.h"
#include "TestSupport.h"

using dv::ErrorCode;
using dv::TagEntry;
using dv::TagGroup;
using namespace dv::test;

namespace {

std::vector<TagEntry> metadataOrFail(const std::filesystem::path& path)
{
    auto entries = dv::readMetadata(path);
    REQUIRE(entries.has_value());
    return *std::move(entries);
}

const TagEntry* find(const std::vector<TagEntry>& entries, std::uint16_t group, std::uint16_t element, int depth = 0)
{
    const auto it = std::ranges::find_if(entries, [&](const TagEntry& e) {
        return e.group == group && e.element == element && e.depth == depth;
    });
    return it == entries.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("metadata lists the expected tags with names, VRs and values", "[metadata]")
{
    const auto entries = metadataOrFail(dataFile("MR_small.dcm"));

    const TagEntry* name = find(entries, 0x0010, 0x0010);
    REQUIRE(name != nullptr);
    CHECK(name->name == "Patient Name");
    CHECK(name->vr == "PN");
    CHECK(name->value == "CompressedSamples^MR1");
    CHECK_FALSE(name->truncated);

    const TagEntry* rows = find(entries, 0x0028, 0x0010);
    REQUIRE(rows != nullptr);
    CHECK(rows->name == "Rows");
    CHECK(rows->vr == "US");
    CHECK(rows->value == "64");

    const TagEntry* pixelSpacing = find(entries, 0x0028, 0x0030);
    REQUIRE(pixelSpacing != nullptr);
    CHECK_THAT(pixelSpacing->value, Catch::Matchers::ContainsSubstring("\\")); // two values

    const TagEntry* modality = find(entries, 0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->value == "MR");
}

TEST_CASE("metadata includes the file meta header", "[metadata]")
{
    const auto entries = metadataOrFail(dataFile("MR_small.dcm"));
    const TagEntry* syntax = find(entries, 0x0002, 0x0010);
    REQUIRE(syntax != nullptr);
    CHECK(syntax->value == "1.2.840.10008.1.2.1");
    CHECK(entries.front().group == 0x0002);
}

TEST_CASE("pixel data is summarised, not dumped", "[metadata]")
{
    const auto entries = metadataOrFail(dataFile("MR_small.dcm"));
    const TagEntry* pixels = find(entries, 0x7FE0, 0x0010);
    REQUIRE(pixels != nullptr);
    CHECK(pixels->name == "Pixel Data");
    CHECK(pixels->value == "[binary 8192 bytes]");
}

TEST_CASE("top-level entries are sorted by tag", "[metadata]")
{
    const char* name = GENERATE("MR_small.dcm", "CT_small.dcm", "MR_small_RLE.dcm");
    const auto entries = metadataOrFail(dataFile(name));
    std::uint32_t previous = 0;
    for (const TagEntry& e : entries) {
        if (e.depth != 0)
            continue;
        const std::uint32_t tag = (std::uint32_t{e.group} << 16) | e.element;
        REQUIRE(tag >= previous);
        previous = tag;
    }
}

TEST_CASE("sequences are flattened with increasing depth", "[metadata]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "seq.dcm", [](DcmDataset& ds) {
        auto* sequence = new DcmSequenceOfItems(DCM_ReferencedStudySequence);
        auto* item = new DcmItem();
        item->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.5.1.4.1.1.4");
        auto* inner = new DcmSequenceOfItems(DCM_ReferencedSeriesSequence);
        auto* innerItem = new DcmItem();
        innerItem->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4");
        inner->insert(innerItem);
        item->insert(inner);
        sequence->insert(item);
        ds.insert(sequence);
    });

    const auto entries = metadataOrFail(dir / "seq.dcm");
    const TagEntry* seq = find(entries, 0x0008, 0x1110);
    REQUIRE(seq != nullptr);
    CHECK(seq->vr == "SQ");
    CHECK_THAT(seq->value, Catch::Matchers::ContainsSubstring("1 item"));

    const TagEntry* uid = find(entries, 0x0008, 0x1150, 1);
    REQUIRE(uid != nullptr);
    CHECK(uid->value == "1.2.840.10008.5.1.4.1.1.4");
    const TagEntry* nestedSeq = find(entries, 0x0008, 0x1115, 1);
    REQUIRE(nestedSeq != nullptr);
    const TagEntry* deepUid = find(entries, 0x0020, 0x000E, 2);
    REQUIRE(deepUid != nullptr);
    CHECK(deepUid->value == "1.2.3.4");

    // Items follow their sequence element immediately.
    const auto seqIndex = seq - entries.data();
    CHECK(entries[static_cast<std::size_t>(seqIndex) + 1].depth == 1);
}

TEST_CASE("long values are truncated and flagged", "[metadata]")
{
    const TempDir dir;
    const std::string longText(1000, 'x');
    rewrite(dataFile("MR_small.dcm"), dir / "long.dcm",
            [&](DcmDataset& ds) { ds.putAndInsertString(DCM_ImageComments, longText.c_str()); });

    const auto entries = metadataOrFail(dir / "long.dcm");
    const TagEntry* comments = find(entries, 0x0020, 0x4000);
    REQUIRE(comments != nullptr);
    CHECK(comments->truncated);
    CHECK(comments->value.size() < longText.size());
    CHECK(comments->value.ends_with("..."));
}

TEST_CASE("metadata is available for files that cannot be displayed", "[metadata]")
{
    const TempDir dir;
    rewrite(dataFile("MR_small.dcm"), dir / "nopixels.dcm", [](DcmDataset& ds) { ds.findAndDeleteElement(DCM_PixelData); });
    const auto noPixels = metadataOrFail(dir / "nopixels.dcm");
    CHECK(find(noPixels, 0x0010, 0x0010) != nullptr);
    CHECK(find(noPixels, 0x7FE0, 0x0010) == nullptr);

    const auto j2k = metadataOrFail(dataFile("JPEG2000.dcm"));
    CHECK(find(j2k, 0x0002, 0x0010) != nullptr);
    CHECK(find(j2k, 0x7FE0, 0x0010) != nullptr);
}

TEST_CASE("metadata of a loaded file matches reading it by path", "[metadata]")
{
    const auto file = dv::DicomFile::load(dataFile("MR_small_RLE.dcm"));
    REQUIRE(file.has_value());
    const auto fromFile = dv::readMetadata(*file);
    REQUIRE(fromFile.has_value());
    const TagEntry* rows = find(*fromFile, 0x0028, 0x0010);
    REQUIRE(rows != nullptr);
    CHECK(rows->value == "64");
    CHECK(find(*fromFile, 0x7FE0, 0x0010) != nullptr);
}

TEST_CASE("metadata errors", "[metadata][error]")
{
    CHECK(dv::readMetadata(dataFile("missing.dcm")).error().code == ErrorCode::FileNotFound);

    const TempDir dir;
    const std::string text = "not dicom at all";
    writeBytes(dir / "text.dcm", {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
    CHECK(dv::readMetadata(dir / "text.dcm").error().code == ErrorCode::NotDicom);
}

TEST_CASE("groupOf categorises tags for the UI", "[metadata]")
{
    CHECK(dv::groupOf(0x0010) == TagGroup::Patient);
    CHECK(dv::groupOf(0x0008) == TagGroup::StudySeries);
    CHECK(dv::groupOf(0x0020) == TagGroup::StudySeries);
    CHECK(dv::groupOf(0x0028) == TagGroup::Image);
    CHECK(dv::groupOf(0x7FE0) == TagGroup::Other);
    CHECK(dv::groupOf(TagEntry{.group = 0x0010, .element = 0x0020}) == TagGroup::Patient);
}
