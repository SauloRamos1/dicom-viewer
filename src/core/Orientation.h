#pragma once

#include <array>
#include <optional>
#include <string>

#include "DicomFile.h"

namespace dv {

// Patient-orientation letters shown at the four image edges: R/L (right/left),
// A/P (anterior/posterior), H/F (head/feet). Oblique planes combine letters in
// decreasing order of significance, e.g. "RA".
struct OrientationLabels {
    std::string left;
    std::string right;
    std::string top;
    std::string bottom;

    friend bool operator==(const OrientationLabels&, const OrientationLabels&) = default;
};

// Labels from Image Orientation Patient (row cosines, then column cosines), in
// the DICOM LPS patient coordinate system (PS3.3 C.7.6.2.1.1). The row vector
// points to the right edge and the column vector to the bottom edge. Secondary
// letters appear for direction cosines above 0.2. Returns nullopt for a
// degenerate (zero length) vector.
[[nodiscard]] std::optional<OrientationLabels> orientationLabels(const std::array<double, 6>& iop);

// Same, from the file's Image Orientation Patient; nullopt when the tag is missing.
[[nodiscard]] std::optional<OrientationLabels> orientationLabels(const ImageInfo& info);

} // namespace dv
