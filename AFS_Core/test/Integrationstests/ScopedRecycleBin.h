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
    std::filesystem::path path_ = std::filesystem::temp_directory_path() / ("autofilesync_recyclebin_" + std::to_string(getpid()));
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
