// Colormap lookup tables, built once from piecewise-linear control points
// (the same anchors as the matplotlib "bone", "hot" and "jet" maps).

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "Windowing.h"

namespace dv {

namespace {

struct Anchor {
    double position;
    double value;
};

std::uint8_t interpolate(std::initializer_list<Anchor> anchors, double x)
{
    const Anchor* previous = anchors.begin();
    for (const Anchor& next : anchors) {
        if (x <= next.position) {
            const double span = next.position - previous->position;
            const double t = span > 0.0 ? (x - previous->position) / span : 1.0;
            const double y = previous->value + t * (next.value - previous->value);
            return static_cast<std::uint8_t>(std::lround(std::clamp(y, 0.0, 1.0) * 255.0));
        }
        previous = &next;
    }
    return static_cast<std::uint8_t>(std::lround(std::clamp(previous->value, 0.0, 1.0) * 255.0));
}

ColorLut makeLut(Colormap colormap)
{
    ColorLut lut{};
    for (std::size_t i = 0; i < lut.size(); ++i) {
        const double x = static_cast<double>(i) / 255.0;
        switch (colormap) {
        case Colormap::Gray:
            lut[i] = {static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i)};
            break;
        case Colormap::Bone:
            lut[i] = {interpolate({{0, 0}, {0.746032, 0.652778}, {1, 1}}, x),
                      interpolate({{0, 0}, {0.365079, 0.319444}, {0.746032, 0.777778}, {1, 1}}, x),
                      interpolate({{0, 0}, {0.365079, 0.444444}, {1, 1}}, x)};
            break;
        case Colormap::Hot:
            lut[i] = {interpolate({{0, 0}, {0.365079, 1}, {1, 1}}, x),
                      interpolate({{0, 0}, {0.365079, 0}, {0.746032, 1}, {1, 1}}, x),
                      interpolate({{0, 0}, {0.746032, 0}, {1, 1}}, x)};
            break;
        case Colormap::Jet:
            lut[i] = {interpolate({{0, 0}, {0.35, 0}, {0.66, 1}, {0.89, 1}, {1, 0.5}}, x),
                      interpolate({{0, 0}, {0.125, 0}, {0.375, 1}, {0.64, 1}, {0.91, 0}, {1, 0}}, x),
                      interpolate({{0, 0.5}, {0.11, 1}, {0.34, 1}, {0.65, 0}, {1, 0}}, x)};
            break;
        }
    }
    return lut;
}

} // namespace

const ColorLut& colormapLut(Colormap colormap)
{
    static const std::array<ColorLut, 4> luts{makeLut(Colormap::Gray), makeLut(Colormap::Bone),
                                              makeLut(Colormap::Hot), makeLut(Colormap::Jet)};
    return luts[static_cast<std::size_t>(colormap)];
}

} // namespace dv
