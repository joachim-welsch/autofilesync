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
#include <Hex.h>

namespace Utilities {

namespace hex {

namespace {
    constexpr char DIGITS[] = "0123456789abcdef";
}

std::string encoded(std::span<const unsigned char> bytes)
{
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        text += DIGITS[byte >> 4];
        text += DIGITS[byte & 0xF];
    }
    return text;
}

std::string encoded(std::string_view bytes)
{
    return encoded(std::span(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()));
}

}

} // namespace Utilities
