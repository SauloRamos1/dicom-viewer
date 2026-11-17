#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DicomError.h"
#include "Voi.h"

namespace dv {

struct DicomFileState; // DCMTK-backed, defined in the private DicomFileState.h

enum class Photometric {
    Monochrome1,
    Monochrome2,
    Rgb,
    YbrFull,
    YbrFull422,
    PaletteColor,
    Other,
};

// Image attributes read once at load time. Spacing arrays are in the DICOM
// order: pixelSpacing = {row spacing, column spacing}.
struct ImageInfo {
    int rows{};
    int columns{};
    int samplesPerPixel{1};
    int bitsAllocated{};
    int bitsStored{};
    int pixelRepresentation{}; // 0 = unsigned, 1 = two's complement
    Photometric photometric{Photometric::Other};
    int frameCount{1};
    std::optional<std::array<double, 2>> pixelSpacing;
    std::optional<double> sliceThickness;
    std::optional<double> spacingBetweenSlices;
    std::optional<std::array<double, 6>> imageOrientation; // (0020,0037)
    std::optional<std::array<double, 3>> imagePosition;    // (0020,0032)
    std::optional<int> instanceNumber;
    double rescaleSlope{1.0};
    double rescaleIntercept{0.0};
    std::optional<Voi> defaultVoi; // first Window Center / Width pair, if any
    std::string transferSyntaxUid;
    std::string transferSyntaxName;
    std::string modality;
    std::string patientName;
    std::string patientId;
    std::string studyDescription;
    std::string seriesDescription;
    std::string studyDate;
    std::string studyInstanceUid;
    std::string seriesInstanceUid;

    [[nodiscard]] bool isMonochrome() const noexcept
    {
        return photometric == Photometric::Monochrome1 || photometric == Photometric::Monochrome2;
    }
};

// A parsed, decodable DICOM image file. Cheap to copy (shared immutable state)
// and safe to use from several threads: access to the underlying DCMTK dataset
// is serialised internally.
class DicomFile {
public:
    // Loads and validates `path`. Compressed pixel data is decoded here, so a
    // successful result is ready to render. Never throws.
    [[nodiscard]] static std::expected<DicomFile, DicomError> load(const std::filesystem::path& path);

    [[nodiscard]] const ImageInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    // Modality-transformed values (stored * slope + intercept) of one frame of
    // a monochrome image, row-major. Modality LUT sequences are not applied.
    [[nodiscard]] std::expected<std::vector<float>, DicomError> rawModalityValues(int frame) const;

    // Modality-transformed value at column `x`, row `y` of `frame`. O(1).
    [[nodiscard]] std::expected<float, DicomError> valueAt(int x, int y, int frame) const;

    // Internal: DCMTK state shared by the core implementation files.
    [[nodiscard]] DicomFileState& state() const noexcept { return *state_; }

private:
    DicomFile() = default;

    std::filesystem::path path_;
    ImageInfo info_;
    std::shared_ptr<DicomFileState> state_;
};

} // namespace dv
