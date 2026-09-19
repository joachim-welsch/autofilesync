/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncSshTwoWayTests
{
	const std::wstring SOURCE_PATH = TestPaths::scratch_dir() + L"/two_way_ssh/source";
	const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/two_way_ssh/target";
	const std::string REMOTE_DIR_NAME = "afs_two_way_ssh";  // [SyncSshTwoWayTests.md#remote-dir-per-test]
}
