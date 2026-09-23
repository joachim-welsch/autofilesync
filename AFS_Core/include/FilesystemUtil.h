/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once
#include <filesystem>
#include <string>
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
}
namespace fsu = FilesystemUtil;

} // namespace AFS_Core
