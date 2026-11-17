// SPDX-License-Identifier: GPL-3.0-only
#include "Text.h"

#include <cctype>
#include <cstddef>

namespace dv {

namespace {

std::string fromLatin1(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x80) {
            out += c;
        } else {
            out += static_cast<char>(0xC0 | (byte >> 6));
            out += static_cast<char>(0x80 | (byte & 0x3F));
        }
    }
    return out;
}

// "ISO 2022 IR 100" and "ISO_IR 100" name the same repertoire; blanks and case do not matter.
// Of a multi-valued set (code extension) only the first value is looked at.
bool isLatin1(std::string_view charset)
{
    std::string normalized;
    for (const char c : charset.substr(0, charset.find('\\'))) {
        if (c != ' ')
            normalized += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return normalized == "ISO_IR100" || normalized == "ISO2022IR100";
}

} // namespace

bool isValidUtf8(std::string_view text) noexcept
{
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        char32_t minimum = 0;
        char32_t code = 0;
        if (lead < 0x80) {
            ++i;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF) {
            extra = 1;
            minimum = 0x80;
            code = lead & 0x1F;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            extra = 2;
            minimum = 0x800;
            code = lead & 0x0F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            extra = 3;
            minimum = 0x10000;
            code = lead & 0x07;
        } else {
            return false;
        }
        if (i + extra >= text.size())
            return false;
        for (std::size_t k = 1; k <= extra; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (next & 0x3F);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
            return false;
        i += extra + 1;
    }
    return true;
}

std::string toUtf8(std::string_view text, std::string_view specificCharacterSet)
{
    if (!isLatin1(specificCharacterSet) && isValidUtf8(text))
        return std::string(text);
    return fromLatin1(text);
}

} // namespace dv
