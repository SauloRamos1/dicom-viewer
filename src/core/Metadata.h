#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "DicomError.h"
#include "DicomFile.h"

namespace dv {

// One DICOM element. Sequences are flattened: the sequence element is
// followed by the elements of its items at depth + 1.
struct TagEntry {
    std::uint16_t group{};
    std::uint16_t element{};
    std::string name;
    std::string vr;
    std::string value;      // display text; binary data shown as "[binary N bytes]"
    bool truncated{false};  // value was shortened for display
    int depth{};
};

// UI grouping of tags.
enum class TagGroup { Patient, StudySeries, Image, Other };

// Patient = group 0010, Study/Series = groups 0008 and 0020, Image = group 0028.
[[nodiscard]] constexpr TagGroup groupOf(std::uint16_t group) noexcept
{
    switch (group) {
    case 0x0010: return TagGroup::Patient;
    case 0x0008:
    case 0x0020: return TagGroup::StudySeries;
    case 0x0028: return TagGroup::Image;
    default: return TagGroup::Other;
    }
}

[[nodiscard]] constexpr TagGroup groupOf(const TagEntry& entry) noexcept
{
    return groupOf(entry.group);
}

// Reads every element of the file (meta header included, in tag order) without
// decoding pixel data, so it works for files load() rejects (no pixel data,
// unsupported transfer syntax).
[[nodiscard]] std::expected<std::vector<TagEntry>, DicomError> readMetadata(
    const std::filesystem::path& path);

[[nodiscard]] std::expected<std::vector<TagEntry>, DicomError> readMetadata(const DicomFile& file);

} // namespace dv
