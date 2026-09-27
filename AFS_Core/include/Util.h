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
#include <string>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <Logging.h>
#include <StrUtil.h>

namespace AFS_Core {

namespace util {

    unsigned long current_pid();

    uint16_t to_uint16_t(const std::wstring& s);  // [Util.md#to_uint16_t]

    std::wstring to_string(bool b);
    std::string to_string(const std::filesystem::file_time_type& ft);

    std::time_t to_time_t(const std::filesystem::file_time_type& ft);  // [Util.md#file-time-conversions]
    std::filesystem::file_time_type to_file_time(std::time_t tt);

    std::wstring with_trailing_char(const std::wstring& s, wchar_t c);

    bool volume_exists(const std::wstring& drive_name, const std::wstring& volume_name);
    bool drive_exists(const std::wstring& drive_name);
    std::filesystem::path with_timestamp_prefix(const std::filesystem::path& pathname,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now());  // [Util.md#with_timestamp_prefix]
    void handle_last_error(std::filesystem::filesystem_error const& ex);
    bool wildmatch(const std::wstring& input, const std::wstring& wildcard);
    bool files_equal(const std::wstring& input1, const std::wstring& input2);
    bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
    bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
    bool rename_file(const std::wstring& oldname, const std::wstring& newname);
    bool remove_file(const std::wstring& name, LogLevel ll = LogLevel::None);
    bool set_last_write_time(const std::wstring& fn, const std::filesystem::file_time_type& modify_time, LogLevel ll = LogLevel::None);
} // namespace util

} // namespace AFS_Core
