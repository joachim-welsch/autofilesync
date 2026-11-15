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
#pragma once
#include <UtilitiesConfig.h>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Utilities {

namespace util {

    std::string wstring_to_string(const std::wstring& ws);
    std::wstring string_to_wstring(const std::string& s);

    uint16_t to_uint16_t(const std::wstring& s);  // [StrUtil.md#to_uint16_t]
    std::wstring to_string(bool b);
    std::wstring to_string(uint16_t i);
    std::wstring to_string(int i);
    template<std::integral T> std::wstring to_string(T) = delete;  // otherwise other integers convert silently to bool
    std::string to_string(const std::filesystem::file_time_type& ft);  // [StrUtil.md#to_string-file-time]

    std::string trimmed(std::string_view text);  // [StrUtil.md#trimmed]
    std::wstring trimmed(std::wstring_view text);
    std::string lowercase(std::string text);  // [StrUtil.md#lowercase]
    std::wstring lowercase(std::wstring text);
    std::string joined(const std::vector<std::string>& parts, std::string_view separator);
    std::wstring joined(const std::vector<std::wstring>& parts, std::wstring_view separator);
    std::vector<std::string> split(const std::string& text, char separator);  // [StrUtil.md#split]
    std::string unquoted(std::string_view text);  // [StrUtil.md#unquoted]
    std::wstring with_trailing_char(const std::wstring& s, wchar_t c);
    std::string without_trailing_chars(std::string s, char c);
    bool wildmatch(const std::wstring& input, const std::wstring& wildcard);  // [StrUtil.md#wildmatch]
}

} // namespace Utilities
