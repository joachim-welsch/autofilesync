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
