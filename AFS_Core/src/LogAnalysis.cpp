/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include "LogAnalysis.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <Logging.h>
#include "Util.h"

namespace AFS_Core {

using util::string_to_wstring;
using util::wstring_to_string;

std::wstring read_recent_log_entries(std::size_t max_chars)
{
    const std::wstring path = Logging::instance().get_log_file_path_name();
    std::ifstream is(wstring_to_string(path), std::ios::in | std::ios::binary);
    if (!is.is_open())
        return {};
    is.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(is.tellg());
    const std::size_t start = size > max_chars ? size - max_chars : 0;
    is.seekg(static_cast<std::streamoff>(start));
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    if (start > 0) {
        const auto first_newline = content.find('\n');
        content.erase(0, first_newline == std::string::npos ? content.size() : first_newline + 1);
    }
    return string_to_wstring(content);
}

bool looks_like_log_question(const std::wstring& text)
{
    std::wstring lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return lower.find(L"log") != std::wstring::npos || lower.find(L"protokoll") != std::wstring::npos;
}

} // namespace AFS_Core
