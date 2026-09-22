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
#include "pch.h"
#include "MountedDrives.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "Logging.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#ifdef WIN32
    #include "Util.h"
    #include <Windows.h>
#else
    #include <sys/stat.h>
    #include "StrUtil.h"
#endif
#ifdef _MACOS
    #include <sys/param.h>
    #include <sys/mount.h>
#endif

namespace AFS_Core {

using std::string;
using std::wstring;
using namespace std::chrono;
namespace fs=std::filesystem;

MountedDrives& MountedDrives::instance()
{
    static MountedDrives obj;
    return obj;
}

bool MountedDrives::time_elapsed() const
{
    return system_clock::now() - check_time_ > 10s;
}

#ifdef WIN32
    fs::path get_mount_point(const fs::path& path)
    {
        wchar_t volume_path_name[MAX_PATH];
        if (GetVolumePathNameW(path.c_str(), volume_path_name, MAX_PATH)) {
            return fs::path(volume_path_name);
        }
        else {
            DWORD error = GetLastError();
            log_error(translation::translate("mount_point_query_error") + std::to_wstring(error));
            return fs::path();
        }
    }
#else
struct FstabEntry {
    std::string device;
    std::string mountpoint;
    std::string fstype;
    std::string options;
    int dump;
    int pass;
};

std::vector<FstabEntry> parse_fstab(const std::string& fstab_path)
{
    std::vector<FstabEntry> entries;
    std::ifstream file(fstab_path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream iss(line);
        FstabEntry entry;
        if (iss >> entry.device >> entry.mountpoint >> entry.fstype >> entry.options >> entry.dump >> entry.pass)
            entries.push_back(entry);
    }
    return entries;
}

bool MountedDrives::mount_point_exist(const string& mp) const
{
    for (const MountInfo& i: mounted_drives_)
        if (i.mount_point == mp)
            return true;
    return false;
}

fs::path get_mount_point(const fs::path& path)
{
    return fsu::get_mount_root(path).value_or(fs::path());
}

bool MountedDrives::check(const fs::path& path) const
{
    log_debug(L"MountedDrives::check");
    const auto mount_root = fsu::get_mount_root(path);
    for (const MountInfo& i : mounted_drives_)
        if (i.mount_point == mount_root)
            return true;
    return false;
}
#endif

#if defined(_UNIX) && !defined(_MACOS)
bool MountedDrives::refresh(const string& mount_point, const std::wstring&)
{
    log_debug(L"MountedDrives::refresh");
    std::ifstream mounts_file("/proc/mounts");
    std::string line;
    mounted_drives_.clear();
    requested_drives_.clear();
    check_time_ = system_clock::now();
    bool retval = false;
    while (std::getline(mounts_file, line)) {
        std::istringstream iss(line);
        MountInfo mount_info;
        if (iss >> mount_info.device >> mount_info.mount_point >> mount_info.file_system_type) {
            mounted_drives_.push_back(mount_info);
            if (mount_info.mount_point == mount_point) {
                requested_drives_.push_back(mount_info);
                check_time_ = system_clock::now();
                retval = true;
                continue;
            }
        }
    }
    return retval;
}

std::optional<std::string> MountedDrives::fstab_mountpoint_for(const fs::path& path) const  // [MountedDrives.md#fstab_mountpoint_for]
{
    const std::string p = path.string();
    std::optional<std::string> best;
    for (const auto& entry : parse_fstab(fstab_path_)) {
        const std::string& mp = entry.mountpoint;
        if (mp == "/" || mp.empty() || mp[0] != '/')
            continue;
        const bool is_prefix = p == mp
            || (p.size() > mp.size() && p.compare(0, mp.size(), mp) == 0 && p[mp.size()] == '/');
        if (is_prefix && (!best || mp.size() > best->size()))
            best = mp;
    }
    return best;
}

bool MountedDrives::mountpoint_active(const std::string& mountpoint) const
{
    std::ifstream mounts("/proc/mounts");
    std::string line, device, mount;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        if (iss >> device >> mount && mount == mountpoint)
            return true;
    }
    return false;
}

#elif defined(_MACOS)
bool MountedDrives::refresh(const string& mount_point, const std::wstring&)  // [MountedDrives.md#macos-refresh]
{
    log_debug(L"MountedDrives::refresh");
    mounted_drives_.clear();
    requested_drives_.clear();
    check_time_ = system_clock::now();
    bool retval = false;
    struct statfs* mounts = nullptr;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < count; ++i) {
        MountInfo mount_info;
        mount_info.device = mounts[i].f_mntfromname;
        mount_info.mount_point = mounts[i].f_mntonname;
        mount_info.file_system_type = mounts[i].f_fstypename;
        mounted_drives_.push_back(mount_info);
        if (mount_info.mount_point == mount_point) {
            requested_drives_.push_back(mount_info);
            check_time_ = system_clock::now();
            retval = true;
        }
    }
    return retval;
}

#else
wstring volume_name_from_driveletter(const wstring& driveletter)
{
	wstring volume_name(MAX_PATH, L'\0');
	DWORD volume_name_size = static_cast<DWORD>(volume_name.size());
	if (GetVolumeInformationW(driveletter.c_str(), &volume_name[0], volume_name_size, NULL, NULL, NULL, NULL, 0) == 0) {
		log_debug(L"No volume information for " + driveletter);  // [MountedDrives.md#drive-without-medium]
		return L"";
	}
	volume_name.resize(wcslen(volume_name.c_str()));
	return volume_name;
}

bool MountedDrives::mount_info_exists(const MountInfo& mount_info) const
{
    return std::find(requested_drives_.begin(), requested_drives_.end(), mount_info) != requested_drives_.end();
}

bool MountedDrives::refresh(const string& mount_point, const std::wstring& name)
{
	log_debug(L"MountedDrives::refresh");
    mounted_drives_.clear();
    check_time_ = std::chrono::system_clock::now();

    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        log_error(L"Failed to get logical drives");
        return false;
    }
    bool retval = false;
    char drive_letter = 'A';
    while (drives) {
        if (drives & 1) {
            std::wstring drive = std::wstring(1, drive_letter) + L":\\";
            UINT drive_type = GetDriveType(drive.c_str());
            if (drive_type != DRIVE_NO_ROOT_DIR) {
                MountInfo mount_info;
                mount_info.device = util::wstring_to_string(drive);
				mount_info.mount_point = mount_info.device;

                switch (drive_type) {
                case DRIVE_REMOVABLE:
                    mount_info.file_system_type = "Removable";
                    break;
                case DRIVE_FIXED:
                    mount_info.file_system_type = "Fixed";
                    break;
                case DRIVE_REMOTE:
                    mount_info.file_system_type = "Remote";
                    break;
                case DRIVE_CDROM:
                    mount_info.file_system_type = "CD-ROM";
                    break;
                case DRIVE_RAMDISK:
                    mount_info.file_system_type = "RAM Disk";
                    break;
                default:
                    mount_info.file_system_type = "Unknown";
                    break;
                }
                if (mount_info.mount_point == mount_point)  // [MountedDrives.md#requested-drive-only]
                    mount_info.name = volume_name_from_driveletter(drive);
                mounted_drives_.push_back(mount_info);
                if (mount_info.mount_point == mount_point && (name.empty() || mount_info.name == name)) {
                    if (!mount_info_exists(mount_info))
					    requested_drives_.push_back(mount_info);
					check_time_ = system_clock::now();
					retval = true;
				}
            }
        }
        drives >>= 1;
        drive_letter++;
    }
	return retval;
}

bool MountedDrives::check(const fs::path& path) const
{
    log_debug(L"MountedDrives::check");
    auto mp = fsu::get_mount_root(path);
    return mp.has_value();
}

#endif

bool MountedDrives::drive_mounted(const fs::path& path, const std::wstring& name)
{
    log_debug(L"MountedDrives::drive_mounted");
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(_UNIX) && !defined(_MACOS)
    if (auto fstab_mountpoint = fstab_mountpoint_for(path))
        return mountpoint_active(*fstab_mountpoint);
#endif
    if (!time_elapsed())
        return check(path);
    const auto mount_root = fsu::get_mount_root(path);
    return mount_root && refresh(mount_root->string(), name);
}

#ifdef _TEST
void MountedDrives::set_test_time(std::chrono::seconds duration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    check_time_= system_clock::now() + duration;
}

bool MountedDrives::time_elapsed_test() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return time_elapsed();
}
#endif

} // namespace AFS_Core
