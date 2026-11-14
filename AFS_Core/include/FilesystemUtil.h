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
#include <string>
#include <string_view>
#include <optional>

namespace AFS_Core {

namespace FilesystemUtil
{
	enum class FileInfoResult { OK, FileNotExist, DriveNotMounted, CannotReadFileAttributes, InvalidFileTime, ErrorReadFileInfo };

	bool file_exists(const std::wstring& filename);
    FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
	std::wstring make_error_message(FileInfoResult result);
    bool touch(const std::wstring& fpathname);
    std::optional<std::filesystem::path> is_mountpoint(const std::filesystem::path& path);
    std::optional<std::filesystem::path> get_mount_root(const std::filesystem::path& in);
	std::string home_dir();
    std::optional<std::filesystem::path> env_path(const char* name);  // [FilesystemUtil.md#env_path]
    bool plain_entry_name(std::string_view name);  // [FilesystemUtil.md#plain_entry_name]
}
namespace fsu = FilesystemUtil;

} // namespace AFS_Core
