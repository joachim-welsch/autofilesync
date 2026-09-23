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
#include <chrono>
#include <filesystem>
#include "SyncFileTests.h"

namespace tu {
	void write_text_file(const std::filesystem::path& file, const std::string& content,
		std::chrono::seconds age = std::chrono::seconds(0));  // [FileUtil.md#write_text_file]
	std::string read_text_file(const std::filesystem::path& file);
	bool copy_file(const std::wstring& source, const std::wstring& destination);
	bool copy_source_to_dest(const std::wstring& source = SyncFileTests::SOURCE_PATH_NAME, const std::wstring& destination = SyncFileTests::TARGET_PATH_NAME);
	bool erase_test_file(const std::wstring& fn);
	std::wstring temp_dir_w();
	std::string temp_dir();
	std::string test_temp_dir();
	std::string test_source_dir();
	std::string test_dest_dir();
	bool path_contains(std::wstring haystack, std::wstring needle);
}