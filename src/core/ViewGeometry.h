// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <optional>

#include "DicomFile.h"
#include "Orientation.h"

namespace dv {

// Physical size of a pixel in millimetres, as {row spacing, column spacing}
// (the DICOM order): Pixel Spacing, else Imager Pixel Spacing (projection
// radiography). nullopt when neither is present with two positive finite values.
[[nodiscard]] std::optional<std::array<double, 2>> physicalPixelSpacing(const ImageInfo& info);

// Width over height of one pixel, so a displayed image is columns * aspect wide
// for every `rows` high. Taken from the physical spacing (column spacing / row
// spacing: the row spacing is the distance between rows, the pixel height),
// else from Pixel Aspect Ratio (vertical : horizontal size), else 1. A source
// with a zero, negative or non-finite value is skipped.
[[nodiscard]] double pixelAspect(const ImageInfo& info);

// How the 2D view shows the image, everything its mouse mapping needs. The
// viewport holds the image fitted (aspect preserved, pixel aspect applied) and
// centred; it is then turned/mirrored by `transform` and scaled by `zoom` about
// the viewport centre, and moved by the pan offset (screen pixels).
struct ViewMapping {
    double viewportWidth{};
    double viewportHeight{};
    int columns{};
    int rows{};
    double pixelAspect{1.0};
    ViewTransform transform;
    double zoom{1.0};
    double panX{};
    double panY{};
};

struct PixelPos {
    int x{};
    int y{};

    friend constexpr bool operator==(const PixelPos&, const PixelPos&) = default;
};

// The image pixel (column x, row y) drawn under screen point (sx, sy), in
// viewport coordinates; nullopt outside the image or for a degenerate view.
[[nodiscard]] std::optional<PixelPos> screenToPixel(const ViewMapping& view, double sx, double sy);

} // namespace dv
