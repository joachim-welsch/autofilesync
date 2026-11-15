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
#include <StrUtil.h>
#include <FileTime.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#ifdef WIN32
   #include <windows.h>
#endif

namespace Utilities {

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

namespace {
    std::tm utc_time(std::time_t tt)
    {
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        return tm;
    }

    std::wregex wildcard_to_regex(const std::wstring& wildcard, bool case_sensitive = true)
    {
        std::wstring regex;  // [StrUtil.md#wildcard-escaping]
        regex.reserve(wildcard.size() * 2);
        for (wchar_t c : wildcard) {
            switch (c) {
            case L'*': regex += L".*"; break;
            case L'?': regex += L'.'; break;
            case L'.': case L'^': case L'$': case L'|': case L'(': case L')':
            case L'[': case L']': case L'{': case L'}': case L'+': case L'\\':
                regex += L'\\'; regex += c; break;
            default: regex += c;
            }
        }
        return std::wregex(regex, case_sensitive ? std::regex_constants::ECMAScript : std::regex_constants::icase);
    }

    template<typename Char>
    std::basic_string<Char> trimmed_text(std::basic_string_view<Char> text, std::basic_string_view<Char> whitespace)
    {
        const auto first = text.find_first_not_of(whitespace);
        if (first == text.npos)
            return {};
        return std::basic_string<Char>(text.substr(first, text.find_last_not_of(whitespace) - first + 1));
    }

    template<typename Char>
    std::basic_string<Char> joined_text(const std::vector<std::basic_string<Char>>& parts, std::basic_string_view<Char> separator)
    {
        std::basic_string<Char> result;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0)
                result += separator;
            result += parts[i];
        }
        return result;
    }
}

uint16_t to_uint16_t(const std::wstring& s)
{
    const int value = std::stoi(s);
    if (value < 0 || value > std::numeric_limits<uint16_t>::max())
        throw std::out_of_range("value outside 0..65535: " + wstring_to_string(s));
    return static_cast<uint16_t>(value);
}

std::wstring to_string(bool b)
{
    return b ? L"true" : L"false";
}

std::wstring to_string(uint16_t i)
{
    return std::to_wstring(i);
}

std::wstring to_string(int i)
{
    return std::to_wstring(i);
}

std::string to_string(const std::filesystem::file_time_type& ft)
{
    const std::tm tm = utc_time(file_time::to_time_t(ft));
    std::stringstream buffer;
    buffer << std::put_time(&tm, "%A, %d %B %Y %H:%M");
    return buffer.str();
}

std::string trimmed(std::string_view text)
{
    return trimmed_text<char>(text, " \t\r\n");
}

std::wstring trimmed(std::wstring_view text)
{
    return trimmed_text<wchar_t>(text, L" \t\r\n");
}

std::string lowercase(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::wstring lowercase(std::wstring text)
{
    std::ranges::transform(text, text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return text;
}

std::string joined(const std::vector<std::string>& parts, std::string_view separator)
{
    return joined_text<char>(parts, separator);
}

std::wstring joined(const std::vector<std::wstring>& parts, std::wstring_view separator)
{
    return joined_text<wchar_t>(parts, separator);
}

std::vector<std::string> split(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::istringstream stream(text);
    for (std::string part; std::getline(stream, part, separator);)
        if (!part.empty())
            parts.push_back(part);
    return parts;
}

std::string unquoted(std::string_view text)
{
    const bool quoted = text.size() >= 2 && text.front() == '"' && text.back() == '"';
    return std::string(quoted ? text.substr(1, text.size() - 2) : text);
}

std::wstring with_trailing_char(const std::wstring& s, wchar_t c)
{
    if (!s.empty() && s.back() == c)  // [StrUtil.md#with_trailing_char-empty-input]
        return s;
    return s + c;
}

std::string without_trailing_chars(std::string s, char c)
{
    while (s.ends_with(c))
        s.pop_back();
    return s;
}

bool wildmatch(const std::wstring& input, const std::wstring& wildcard)
{
    return std::regex_match(input, wildcard_to_regex(wildcard));
}

}

} // namespace Utilities
