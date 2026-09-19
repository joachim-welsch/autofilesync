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
#include <vector>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>

namespace AFS_Core {

struct MountInfo {
    std::string device;
    std::string mount_point;
    std::string file_system_type;
    std::wstring name;
    bool operator==(const MountInfo& other) const
    {
        return device == other.device &&
            mount_point == other.mount_point &&
            name == other.name;
    }
};

using MountInfos = std::vector<MountInfo>;
using CheckTime = std::chrono::time_point<std::chrono::system_clock>;

class MountedDrives {
    MountInfos mounted_drives_;
    MountInfos requested_drives_;
    CheckTime  check_time_;
    mutable std::mutex mutex_;  // [MountedDrives.md#mutex_]

    bool refresh(const std::string& device, const std::wstring& name);
    bool time_elapsed() const;
    bool check(const std::filesystem::path& path) const;
    bool mount_point_exist(const std::string& mp) const;
    bool mount_info_exists(const MountInfo& info) const;
#if defined(_UNIX) && !defined(_MACOS)
    std::string fstab_path_ = "/etc/fstab";
    std::optional<std::string> fstab_mountpoint_for(const std::filesystem::path& path) const;
    bool mountpoint_active(const std::string& mountpoint) const;
#endif
#ifdef _TEST
public:
    void set_test_time(std::chrono::seconds);
    bool time_elapsed_test() const;
#if defined(_UNIX) && !defined(_MACOS)
    void set_fstab_path_for_test(const std::string& p) { fstab_path_ = p; }
#endif
#endif
public:
    static MountedDrives& instance();
    bool drive_mounted(const std::filesystem::path& path, const std::wstring& windowsmount=L"");
};

std::filesystem::path get_mount_point(const std::filesystem::path& path);

} // namespace AFS_Core
