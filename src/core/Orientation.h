// SPDX-License-Identifier: GPL-3.0-only
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

// How the viewer shows the image: rotated by `quarterTurns` clockwise (any
// integer, taken modulo 4) and then mirrored on screen horizontally and/or
// vertically.
struct ViewTransform {
    int quarterTurns{};
    bool flipHorizontal{};
    bool flipVertical{};

    friend constexpr bool operator==(const ViewTransform&, const ViewTransform&) = default;
};

// Labels from Image Orientation Patient (row cosines, then column cosines), in
// the DICOM LPS patient coordinate system (PS3.3 C.7.6.2.1.1). The row vector
// points to the right edge and the column vector to the bottom edge. Secondary
// letters appear for direction cosines above 0.2. Returns nullopt for a
// degenerate (zero length) vector.
[[nodiscard]] std::optional<OrientationLabels> orientationLabels(const std::array<double, 6>& iop);

// Same, from the file's Image Orientation Patient; nullopt when the tag is missing.
[[nodiscard]] std::optional<OrientationLabels> orientationLabels(const ImageInfo& info);

// The labels at the four screen edges once `transform` is applied to the image
// that has `labels` (they move with the edge they were attached to).
[[nodiscard]] OrientationLabels transformLabels(const OrientationLabels& labels, const ViewTransform& transform);

} // namespace dv
