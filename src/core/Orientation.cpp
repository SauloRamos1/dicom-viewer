#include "Orientation.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace dv {

namespace {

constexpr double kSecondaryThreshold = 0.2;
constexpr double kMinLength = 1e-6;

using Vec3 = std::array<double, 3>;

// Letters for the direction a unit vector points to in LPS patient space:
// +x = Left, +y = Posterior, +z = Head. Most significant axis first.
std::string letters(const Vec3& v)
{
    constexpr std::array<std::array<char, 2>, 3> names{{{'L', 'R'}, {'P', 'A'}, {'H', 'F'}}};
    std::array<std::size_t, 3> axes{0, 1, 2};
    std::ranges::sort(axes, [&](std::size_t a, std::size_t b) { return std::abs(v[a]) > std::abs(v[b]); });

    std::string result;
    for (const std::size_t axis : axes) {
        if (std::abs(v[axis]) > kSecondaryThreshold)
            result += names[axis][v[axis] > 0 ? 0 : 1];
    }
    return result;
}

Vec3 negated(const Vec3& v)
{
    return {-v[0], -v[1], -v[2]};
}

std::optional<Vec3> normalized(const Vec3& v)
{
    const double length = std::sqrt(std::inner_product(v.begin(), v.end(), v.begin(), 0.0));
    if (length < kMinLength)
        return std::nullopt;
    return Vec3{v[0] / length, v[1] / length, v[2] / length};
}

} // namespace

std::optional<OrientationLabels> orientationLabels(const std::array<double, 6>& iop)
{
    const auto row = normalized({iop[0], iop[1], iop[2]});
    const auto column = normalized({iop[3], iop[4], iop[5]});
    if (!row || !column)
        return std::nullopt;
    return OrientationLabels{.left = letters(negated(*row)),
                             .right = letters(*row),
                             .top = letters(negated(*column)),
                             .bottom = letters(*column)};
}

std::optional<OrientationLabels> orientationLabels(const ImageInfo& info)
{
    if (!info.imageOrientation)
        return std::nullopt;
    return orientationLabels(*info.imageOrientation);
}

} // namespace dv
