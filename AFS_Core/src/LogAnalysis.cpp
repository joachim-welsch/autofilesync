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
#include <filesystem>
#include <fstream>
#include <Logging.h>
#include "Translation.h"
#include "Util.h"
#include <sstream>
#include <vector>

namespace AFS_Core {

using util::string_to_wstring;
using util::wstring_to_string;

std::wstring read_recent_log_entries(std::size_t max_chars)
{
    const std::wstring path = Logging::instance().get_log_file_path_name();
    std::ifstream is(std::filesystem::path(path), std::ios::in | std::ios::binary);  // [Logging.md#wide-file-names]
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

namespace {
std::vector<std::wstring> log_words()  // [LogAnalysis.md#log_words]
{
    std::vector<std::wstring> words{L"log"};
    std::wistringstream list(translation::translate("log_question_words"));
    for (std::wstring word; std::getline(list, word, L',');)
        if (!word.empty())
            words.push_back(word);
    return words;
}
}

bool looks_like_log_question(const std::wstring& text)
{
    std::wstring lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });  // [LogAnalysis.md#towlower]
    const std::vector<std::wstring> words = log_words();
    return std::ranges::any_of(words, [&lower](const std::wstring& word) { return lower.find(word) != std::wstring::npos; });
}

} // namespace AFS_Core
