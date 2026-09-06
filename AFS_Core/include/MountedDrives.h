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
    std::string mountPoint;
    std::string fileSystemType;
    std::wstring name;
    bool operator==(const MountInfo& other) const {
        return device == other.device &&
            mountPoint == other.mountPoint &&
            name == other.name;
    }
};

using MountInfos = std::vector<MountInfo>;
using CheckTime = std::chrono::time_point<std::chrono::system_clock>;

class MountedDrives {
    MountInfos mountedDrives_;
    MountInfos requestedDrives_;
    CheckTime  checkTime_;
    // Guards all three members above: driveMounted() is called from the sync
    // worker thread for every file compared, and this is a process-wide
    // singleton, so any other caller (present or future) races it without
    // this lock. Private helpers below assume the caller already holds it.
    mutable std::mutex mutex_;

    bool refresh(const std::string& device, const std::wstring& name);
    bool timeElapsed() const;
    bool check(const std::filesystem::path& path) const;
    bool mountPointExist(const std::string& mp) const;
    bool mountInfoExists(const MountInfo& info) const;
#if defined(_UNIX) && !defined(_MACOS)
    std::string fstabPath_ = "/etc/fstab";
    std::optional<std::string> fstabMountpointFor(const std::filesystem::path& path) const;
    bool mountpointActive(const std::string& mountpoint) const;
#endif
#ifdef _TEST
public:
    void setTestTime(std::chrono::seconds);
    bool timeElapsedTest() const;
#if defined(_UNIX) && !defined(_MACOS)
    void setFstabPathForTest(const std::string& p) { fstabPath_ = p; }
#endif
#endif
public:
    static MountedDrives& instance();
    bool driveMounted(const std::filesystem::path& path, const std::wstring& windowsmount=L"");
};

std::filesystem::path getMountPoint(const std::filesystem::path& path);

} // namespace AFS_Core
