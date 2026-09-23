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
#include <vector>

namespace tu {
	bool erase_test_dir(const std::wstring& fn, bool display_error);
	void erase_ssh_test_dir(const std::wstring& ssh_path);  // [DirUtil.md#erase_ssh_test_dir]
	void erase_ssh_test_dir_variants(const std::wstring& ssh_base_path, const std::vector<std::wstring>& suffixes);  // [DirUtil.md#erase_ssh_test_dir_variants]
	bool check_copied_files(const std::wstring& source_dir, const std::wstring& dest_dir);
	std::string concat_path(const std::string& path1, const std::string& path2);
	std::wstring concat_path(const std::wstring& path1, const std::wstring& path2);
	std::string concat_temp(const std::string& path);
	std::wstring concat_temp(const std::wstring& path);
	std::string concat_ssh_temp(const std::string& path);
	std::wstring concat_ssh_temp(const std::wstring& path);
}
