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
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace TestPaths  // [TestPaths.md#TestPaths]
{
    inline std::wstring repo_dir()
    {
        static const std::string dir = LIB_SOURCE_DIR;
        return std::wstring(dir.begin(), dir.end());
    }

    inline std::wstring scratch_dir()
    {
        static const std::wstring dir = (std::filesystem::temp_directory_path() / "autofilesync_test").wstring();
        return dir;
    }

    inline int process_id()  // [TestPaths.md#process_id]
    {
#ifdef _WIN32
        return _getpid();
#else
        return getpid();
#endif
    }
}
