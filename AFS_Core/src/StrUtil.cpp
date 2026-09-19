/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include <StrUtil.h>
#ifdef WIN32
   #include <windows.h>
#endif

namespace AFS_Core {

namespace util {

#ifdef WIN32
std::string wstring_to_string(const std::wstring& ws)
{
    if (ws.empty())
        return {};
    const int size = static_cast<int>(ws.size());
    const int need = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), size, nullptr, 0, nullptr, nullptr);
    if (need == 0)
        return {};
    std::string result(static_cast<size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), size, &result[0], need, nullptr, nullptr);
    return result;
}

std::wstring string_to_wstring(const std::string& s)
{
    if (s.empty())
        return {};
    const int size = static_cast<int>(s.size());
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int need = MultiByteToWideChar(codepage, flags, s.c_str(), size, nullptr, 0);
    if (need == 0) {
        codepage = CP_ACP;
        flags = 0;
        need = MultiByteToWideChar(codepage, flags, s.c_str(), size, nullptr, 0);
        if (need == 0)
            return {};
    }
    std::wstring result(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(codepage, flags, s.c_str(), size, &result[0], need);
    return result;
}
#else
namespace {  // [StrUtil.md#utf8-conversions-never-throw]
    constexpr char32_t REPLACEMENT_CHAR = 0xFFFD;

    bool is_continuation(unsigned char c)
    {
        return (c & 0xC0) == 0x80;
    }

    char32_t decode_utf8(const std::string& s, size_t& pos)  // [StrUtil.md#decode_utf8]
    {
        const auto lead = static_cast<unsigned char>(s[pos++]);
        if (lead < 0x80)
            return lead;
        size_t length = 0;
        char32_t code = 0;
        if ((lead & 0xE0) == 0xC0) { length = 1; code = lead & 0x1F; }
        else if ((lead & 0xF0) == 0xE0) { length = 2; code = lead & 0x0F; }
        else if ((lead & 0xF8) == 0xF0) { length = 3; code = lead & 0x07; }
        else return REPLACEMENT_CHAR;
        if (pos + length > s.size())
            return REPLACEMENT_CHAR;
        for (size_t i = 0; i < length; ++i) {
            const auto c = static_cast<unsigned char>(s[pos + i]);
            if (!is_continuation(c))
                return REPLACEMENT_CHAR;
            code = (code << 6) | (c & 0x3F);
        }
        pos += length;
        const bool overlong = code < (length == 1 ? 0x80u : length == 2 ? 0x800u : 0x10000u);
        const bool surrogate = code >= 0xD800 && code <= 0xDFFF;
        return overlong || surrogate || code > 0x10FFFF ? REPLACEMENT_CHAR : code;
    }

    void encode_utf8(char32_t code, std::string& out)
    {
        if ((code >= 0xD800 && code <= 0xDFFF) || code > 0x10FFFF)
            code = REPLACEMENT_CHAR;
        if (code < 0x80)
            out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
}

std::string wstring_to_string(const std::wstring& ws)
{
    std::string result;
    result.reserve(ws.size());
    for (wchar_t c : ws)
        encode_utf8(static_cast<char32_t>(c), result);
    return result;
}

std::wstring string_to_wstring(const std::string& s)
{
    std::wstring result;
    result.reserve(s.size());
    for (size_t pos = 0; pos < s.size();)
        result += static_cast<wchar_t>(decode_utf8(s, pos));
    return result;
}
#endif

}

} // namespace AFS_Core
