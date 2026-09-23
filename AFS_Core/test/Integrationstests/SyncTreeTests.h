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
#include "TestPaths.h"

namespace SyncTreeTests
{
#ifdef _UNIX
	const std::wstring SOURCE_PATH = TestPaths::repo_dir() + L"/test/resources/tree/";
	const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/tree";
	const std::wstring RESOURCE_PATH = TestPaths::repo_dir() + L"/test/tree/source/";
	const std::wstring COUNT_TEST_SOURCE_PATH = TestPaths::scratch_dir() + L"/tree/src/";
	const std::wstring COUNT_TEST_TARGET_PATH = TestPaths::scratch_dir() + L"/tree/dest";
#else
	const std::wstring SOURCE_PATH = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\tree\\";
	const std::wstring TARGET_PATH = L"C:\\temp\\test\\tree";
	const std::wstring RESOURCE_PATH = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\tree\\source\\";
	const std::wstring COUNT_TEST_SOURCE_PATH = L"C:\\temp\\test\\tree\\src\\";
	const std::wstring COUNT_TEST_TARGET_PATH = L"C:\\temp\\test\\tree\\dest";
#endif
}
