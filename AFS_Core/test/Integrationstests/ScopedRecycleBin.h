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
#include <RecycleBin.h>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace tu {

class ScopedRecycleBin {  // [ScopedRecycleBin.md#ScopedRecycleBin]
    std::filesystem::path previous_ = AFS_Core::RecycleBin::instance().bin_path();
    std::filesystem::path path_ = std::filesystem::temp_directory_path() / ("autofilesync_recyclebin_" + std::to_string(_getpid()));
public:
    ScopedRecycleBin()
    {
        std::filesystem::remove_all(path_);
        AFS_Core::RecycleBin::instance().bin_path(path_);
    }
    ScopedRecycleBin(const ScopedRecycleBin&) = delete;
    ScopedRecycleBin& operator=(const ScopedRecycleBin&) = delete;
    ~ScopedRecycleBin()
    {
        AFS_Core::RecycleBin::instance().bin_path(previous_);
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }
};

} // namespace tu
