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
#if defined(_UNIX) && !defined(_MACOS)
#include <MountedDrives.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace tu {

class ScopedFstab {  // [ScopedFstab.md#ScopedFstab]
    std::filesystem::path file_;
public:
    explicit ScopedFstab(std::filesystem::path file) : file_(std::move(file)) {}
    ScopedFstab(const ScopedFstab&) = delete;
    ScopedFstab& operator=(const ScopedFstab&) = delete;
    ~ScopedFstab()
    {
        Utilities::MountedDrives::instance().set_fstab_path_for_test("/etc/fstab");
    }
    void content(const std::string& text) const
    {
        std::ofstream(file_) << text;
        Utilities::MountedDrives::instance().set_fstab_path_for_test(file_.string());
    }
};

} // namespace tu
#endif
