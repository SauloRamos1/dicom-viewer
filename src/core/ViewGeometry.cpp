// SPDX-License-Identifier: GPL-3.0-only
#include "ViewGeometry.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace dv {

namespace {

bool positive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool usable(const std::array<double, 2>& pair)
{
    return positive(pair[0]) && positive(pair[1]);
}

} // namespace

std::optional<std::array<double, 2>> physicalPixelSpacing(const ImageInfo& info)
{
    for (const auto& spacing : {info.pixelSpacing, info.imagerPixelSpacing}) {
        if (spacing && usable(*spacing))
            return spacing;
    }
    return std::nullopt;
}

double pixelAspect(const ImageInfo& info)
{
    // Spacing is {row, column} and the ratio {vertical, horizontal}: both are
    // {height, width} of a pixel.
    if (const auto spacing = physicalPixelSpacing(info))
        return (*spacing)[1] / (*spacing)[0];
    if (info.pixelAspectRatio && usable(*info.pixelAspectRatio))
        return (*info.pixelAspectRatio)[1] / (*info.pixelAspectRatio)[0];
    return 1.0;
}

std::optional<PixelPos> screenToPixel(const ViewMapping& view, double sx, double sy)
{
    if (view.columns <= 0 || view.rows <= 0 || !positive(view.viewportWidth) || !positive(view.viewportHeight)
        || !positive(view.zoom) || !positive(view.pixelAspect) || !std::isfinite(sx) || !std::isfinite(sy)
        || !std::isfinite(view.panX) || !std::isfinite(view.panY))
        return std::nullopt;

    // Undo pan and zoom (both about the viewport centre), leaving the offset from it.
    double u = (sx - view.viewportWidth / 2 - view.panX) / view.zoom;
    double v = (sy - view.viewportHeight / 2 - view.panY) / view.zoom;

    // Undo the mirroring, then the clockwise quarter turns (each undone as a
    // counter-clockwise one: (u, v) -> (v, -u) on a y-down screen).
    if (view.transform.flipHorizontal)
        u = -u;
    if (view.transform.flipVertical)
        v = -v;
    const int quarterTurns = ((view.transform.quarterTurns % 4) + 4) % 4;
    for (int i = 0; i < quarterTurns; ++i)
        std::tie(u, v) = std::pair{v, -u};

    // The image box: the displayed aspect fitted into the (turned) viewport.
    const bool sideways = quarterTurns % 2 != 0;
    const double rotatorWidth = sideways ? view.viewportHeight : view.viewportWidth;
    const double rotatorHeight = sideways ? view.viewportWidth : view.viewportHeight;
    const double displayAspect = view.columns * view.pixelAspect / view.rows;
    const double boxWidth = std::min(rotatorWidth, rotatorHeight * displayAspect);
    const double boxHeight = boxWidth / displayAspect;

    const double column = std::floor((u + boxWidth / 2) / boxWidth * view.columns);
    const double row = std::floor((v + boxHeight / 2) / boxHeight * view.rows);
    if (column < 0 || row < 0 || column >= view.columns || row >= view.rows)
        return std::nullopt;
    return PixelPos{static_cast<int>(column), static_cast<int>(row)};
}

} // namespace dv
