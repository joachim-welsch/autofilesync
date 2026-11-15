/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include <PercentEncoding.h>

namespace Utilities {

namespace percent {

namespace {
    constexpr char DIGITS[] = "0123456789ABCDEF";
    constexpr char MARK = '%';
    constexpr int INVALID = -1;

    bool unreserved(char c)  // [PercentEncoding.md#encoded]
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
    }

    int digit_value(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return c >= 'a' && c <= 'f' ? c - 'a' + 10 : INVALID;
    }

    int byte_at(std::string_view text, std::size_t mark)
    {
        if (mark + 2 >= text.size())
            return INVALID;
        const int high = digit_value(text[mark + 1]);
        const int low = digit_value(text[mark + 2]);
        return high == INVALID || low == INVALID ? INVALID : high << 4 | low;
    }
}

std::string encoded(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char c : text) {
        if (unreserved(c)) {
            result += c;
            continue;
        }
        const auto byte = static_cast<unsigned char>(c);
        result += MARK;
        result += DIGITS[byte >> 4];
        result += DIGITS[byte & 0xF];
    }
    return result;
}

std::string decoded(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const int byte = text[i] == MARK ? byte_at(text, i) : INVALID;
        if (byte == INVALID) {
            result += text[i];
            continue;
        }
        result += static_cast<char>(byte);
        i += 2;
    }
    return result;
}

}

} // namespace Utilities
