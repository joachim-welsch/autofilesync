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