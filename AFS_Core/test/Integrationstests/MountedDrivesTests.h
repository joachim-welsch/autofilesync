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

namespace MountedDrivesTests {
#ifdef _UNIX
    const std::wstring TESTMOUNT = L"/media/winuser/test";
    const std::wstring TESTMOUNTFN = L"/afs_missing_dir/test1.txt"; // [MountedDrivesTests.md#root-mount-point]
    const std::wstring TESTMOUNT2 = L"/";
#else
	const std::wstring TESTMOUNT = L"Z:\\";
	const std::wstring TESTMOUNT2 = L"C:\\";
	const std::wstring TESTMOUNTFN = L"C:\\temp\\test\\test1.txt";
#endif
}

