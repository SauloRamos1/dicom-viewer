// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace dv {

// Value of interest window (DICOM Window Center / Window Width), expressed in
// modality units (e.g. Hounsfield units for CT).
struct Voi {
    double center{};
    double width{};

    friend constexpr bool operator==(const Voi&, const Voi&) = default;
};

} // namespace dv
