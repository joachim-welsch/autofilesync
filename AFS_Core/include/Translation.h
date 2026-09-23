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
#pragma once
#include <filesystem>
#include <initializer_list>
#include <string>

namespace AFS_Core {

namespace translation {  // [Translation.md#translation]
void init_locale(const char* fallback);  // [Translation.md#init_locale]
std::filesystem::path locale_dir();  // [Translation.md#locale_dir]
void init(const std::filesystem::path& locale_dir);  // [Translation.md#init]
constexpr const char* msgid(const char* id) { return id; }  // [Translation.md#msgid]
std::wstring translate(const char* msgid);  // translation of msgid for the language taken from LANGUAGE/LC_MESSAGES
std::string translate_utf8(const char* msgid);  // [Translation.md#translate_utf8]
std::wstring translate(const char* msgid, std::initializer_list<std::wstring> values);  // [Translation.md#placeholders]
std::string translate_utf8(const char* msgid, std::initializer_list<std::string> values);
}

} // namespace AFS_Core
