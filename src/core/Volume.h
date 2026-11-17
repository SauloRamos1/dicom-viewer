#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

#include "DicomError.h"
#include "DicomFile.h"

namespace dv {

// Scalar volume of modality values (e.g. HU). voxels[(z * height + y) * width + x].
// spacing is {x (column), y (row), z (slice)} in millimetres.
struct Volume {
    int width{};
    int height{};
    int depth{};
    std::array<double, 3> spacing{1.0, 1.0, 1.0};
    std::vector<float> voxels;

    // Frames of one multi-frame monochrome file, in frame order.
    [[nodiscard]] static std::expected<Volume, DicomError> fromMultiFrame(const DicomFile& file);

    // Single-frame monochrome slices of one series. Files that are not valid
    // DICOM images or belong to another series than the first valid file are
    // skipped. Slices are sorted by Image Position (Patient) projected on the
    // slice normal, falling back to Instance Number, then input order.
    [[nodiscard]] static std::expected<Volume, DicomError> fromSeries(
        std::span<const std::filesystem::path> paths);
};

} // namespace dv
