// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Private to src/core: size arithmetic for sizes read from untrusted headers.

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>

namespace dv {

// Product of `factors`, or nullopt when it does not fit in std::size_t.
[[nodiscard]] constexpr std::optional<std::size_t> checkedProduct(std::initializer_list<std::size_t> factors) noexcept
{
    std::size_t product = 1;
    for (const std::size_t factor : factors) {
        if (factor != 0 && product > std::numeric_limits<std::size_t>::max() / factor)
            return std::nullopt;
        product *= factor;
    }
    return product;
}

} // namespace dv
