#pragma once

#include <array>
#include <atomic>
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

    // The volume of loadSeries(paths) alone.
    [[nodiscard]] static std::expected<Volume, DicomError> fromSeries(
        std::span<const std::filesystem::path> paths);
};

// A series loaded from a folder: its volume and the files of its slices, one per
// volume slice in the same (depth) order, so a viewer can show slice N as an image.
struct Series {
    Volume volume;
    std::vector<DicomFile> slices;
};

// Single-frame monochrome slices of one series. Files that are not valid DICOM
// images are skipped, and so are files of another series than the most common one
// among the valid files (the first of those on a tie). Slices are sorted by Image
// Position (Patient) projected on the slice normal, falling back to Instance
// Number, then input order. Errors: NoDicomFiles when nothing opens as DICOM (or
// `paths` is empty), the first file's error when it is more specific than
// NotDicom, VolumeInconsistent for multi-frame files or slices of other sizes.
// `cancel`, when set, is checked between files; the result is then an error whose
// detail is "cancelled".
[[nodiscard]] std::expected<Series, DicomError> loadSeries(std::span<const std::filesystem::path> paths,
                                                           const std::atomic<bool>* cancel = nullptr);

} // namespace dv
