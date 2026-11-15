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
#include <Base64.h>
#include <StrUtil.h>
#include <algorithm>

namespace Utilities {

namespace base64 {

namespace {
    constexpr char STANDARD_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    constexpr char URL_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    constexpr char PADDING = '=';
    constexpr std::size_t MAX_PADDING = 2;
    constexpr int INVALID = -1;

    std::string encoded_with(std::span<const unsigned char> bytes, const char* alphabet)
    {
        std::string text;
        text.reserve((bytes.size() + 2) / 3 * 4);
        for (std::size_t i = 0; i < bytes.size(); i += 3) {
            const std::size_t count = std::min<std::size_t>(3, bytes.size() - i);
            const unsigned chunk = bytes[i] << 16 | (count > 1 ? bytes[i + 1] << 8 : 0) | (count > 2 ? bytes[i + 2] : 0);
            text += alphabet[chunk >> 18];
            text += alphabet[chunk >> 12 & 63];
            text += count > 1 ? alphabet[chunk >> 6 & 63] : PADDING;
            text += count > 2 ? alphabet[chunk & 63] : PADDING;
        }
        return text;
    }

    int value_of(char c)
    {
        const std::string_view alphabet = STANDARD_ALPHABET;
        const auto pos = alphabet.find(c);
        return pos == alphabet.npos ? INVALID : static_cast<int>(pos);
    }

    std::size_t padding_of(std::string_view text)
    {
        const auto last = text.find_last_not_of(PADDING);
        return last == text.npos ? text.size() : text.size() - last - 1;
    }
}

std::string encoded(std::span<const unsigned char> bytes)
{
    return encoded_with(bytes, STANDARD_ALPHABET);
}

std::string encoded(std::string_view bytes)
{
    return encoded(std::span(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()));
}

std::string encoded_unpadded(std::span<const unsigned char> bytes)
{
    return util::without_trailing_chars(encoded(bytes), PADDING);
}

std::string url_encoded(std::span<const unsigned char> bytes)
{
    return util::without_trailing_chars(encoded_with(bytes, URL_ALPHABET), PADDING);
}

std::vector<unsigned char> decoded(std::string_view text)
{
    const std::size_t padding = padding_of(text);
    if (text.size() % 4 != 0 || padding > MAX_PADDING)
        return {};
    std::vector<unsigned char> bytes;
    bytes.reserve(text.size() / 4 * 3);
    unsigned buffer = 0;
    int bits = 0;
    for (const char c : text.substr(0, text.size() - padding)) {
        const int value = value_of(c);
        if (value == INVALID)
            return {};
        buffer = buffer << 6 | static_cast<unsigned>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<unsigned char>(buffer >> bits & 0xFF));
        }
    }
    return bytes;
}

}

} // namespace Utilities
