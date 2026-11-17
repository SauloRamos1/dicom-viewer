// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <string>
#include <string_view>

namespace dv {

// True when `text` is well-formed UTF-8 (no overlong forms, surrogates or code points past U+10FFFF).
[[nodiscard]] bool isValidUtf8(std::string_view text) noexcept;

// DICOM text (a value of VR PN, LO, SH, LT, ST or UT) as UTF-8 for display, given the first
// value of Specific Character Set (0008,0005). Without the oficonv data files only the
// repertoires that need no table are converted: ISO_IR 100 (Latin-1) and ISO_IR 192 / ASCII
// (already UTF-8). For any other declared set the text is kept when it is valid UTF-8 and read as
// Latin-1 otherwise, so the result is always valid UTF-8 (possibly with wrong accents). Never throws.
[[nodiscard]] std::string toUtf8(std::string_view text, std::string_view specificCharacterSet = {});

} // namespace dv
