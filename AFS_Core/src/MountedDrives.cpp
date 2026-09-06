#include "pch.h"
#include "MountedDrives.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "Logging.h"
#include <FilesystemUtil.h>
#ifdef WIN32
    #include "util.h"
    #include <Windows.h>
#else
    #include <sys/stat.h>
    #include "strutil.h"
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

bool MountedDrives::timeElapsed() const
{
    auto diff = system_clock::now() - checkTime_;
    auto diff_s = std::chrono::duration_cast<std::chrono::seconds>(diff);
// #ifdef _TEST
//     std::cout << "Time diff: " << diff_s << std::endl;
// #endif
    return diff_s > 10s;
}

#ifdef WIN32
    fs::path getMountPoint(const fs::path& path)
    {
        wchar_t volumePathName[MAX_PATH];
        if (GetVolumePathNameW(path.c_str(), volumePathName, MAX_PATH)) {
            return fs::path(volumePathName);
        }
        else {
            DWORD error = GetLastError();
            LOG_ERROR(L"Fehler beim Abrufen des Mountpoints: " + std::to_wstring(error));
            return fs::path(); // Leerer Pfad bei Fehler
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

// Parse /etc/fstab
std::vector<FstabEntry> parseFstab(const std::string& fstabPath = "/etc/fstab") {
    std::vector<FstabEntry> entries;
    std::ifstream file(fstabPath);
    std::string line;

    while (std::getline(file, line)) {
        // Kommentare und leere Zeilen überspringen
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        FstabEntry entry;
        if (!(iss >> entry.device >> entry.mountpoint >> entry.fstype >> entry.options >> entry.dump >> entry.pass)) {
            continue; // ungültige Zeile
        }
        entries.push_back(entry);
    }
    return entries;
}

bool MountedDrives::mountPointExist(const string& mp) const
{
    for (const MountInfo& i: mountedDrives_)
        if (i.mountPoint == mp)
            return true;
    return false;
}

fs::path getMountPoint(const fs::path& path)
{
    fs::path retval {};
    auto mp = fsu::getMountRoot(path);
    if (mp.has_value())
        retval=mp.value();
    return retval;
}

#endif

#if defined(_UNIX) && !defined(_MACOS)
bool MountedDrives::refresh(const string& mountPoint, const std::wstring&)
{
    LOG_DEBUG(L"MountedDrives::refresh");
    std::ifstream mountsFile("/proc/mounts");
    std::string line;
    mountedDrives_.clear();
    requestedDrives_.clear();
    checkTime_ = system_clock::now();
    bool retval = false;
    while (std::getline(mountsFile, line)) {
        std::istringstream iss(line);
        MountInfo mountInfo;
        if (iss >> mountInfo.device >> mountInfo.mountPoint >> mountInfo.fileSystemType) {
            mountedDrives_.push_back(mountInfo);
            if (mountInfo.mountPoint == mountPoint) {
                requestedDrives_.push_back(mountInfo);
                checkTime_ = system_clock::now();
                retval = true;
                continue;
            }
        }
    }
    return retval;
}

bool MountedDrives::check(const fs::path& path) const
{
    LOG_DEBUG(L"MountedDrives::check");
    for (auto i : mountedDrives_) {
        if (i.mountPoint == fsu::getMountRoot(path))
            return true;
    }
    return false;
}

// Deepest fstab mountpoint (other than "/") that is a path prefix of 'path'.
std::optional<std::string> MountedDrives::fstabMountpointFor(const fs::path& path) const
{
    const std::string p = path.string();
    std::optional<std::string> best;
    for (const auto& entry : parseFstab(fstabPath_)) {
        const std::string& mp = entry.mountpoint;
        if (mp == "/" || mp.empty() || mp[0] != '/')
            continue;
        const bool isPrefix = p == mp
            || (p.size() > mp.size() && p.compare(0, mp.size(), mp) == 0 && p[mp.size()] == '/');
        if (isPrefix && (!best || mp.size() > best->size()))
            best = mp;
    }
    return best;
}

bool MountedDrives::mountpointActive(const std::string& mountpoint) const
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
// macOS has no /proc; the BSD getmntinfo() syscall wrapper returns the
// currently mounted filesystems directly from the kernel instead.
bool MountedDrives::refresh(const string& mountPoint, const std::wstring&)
{
    LOG_DEBUG(L"MountedDrives::refresh");
    mountedDrives_.clear();
    requestedDrives_.clear();
    checkTime_ = system_clock::now();
    bool retval = false;
    struct statfs* mounts = nullptr;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < count; ++i) {
        MountInfo mountInfo;
        mountInfo.device = mounts[i].f_mntfromname;
        mountInfo.mountPoint = mounts[i].f_mntonname;
        mountInfo.fileSystemType = mounts[i].f_fstypename;
        mountedDrives_.push_back(mountInfo);
        if (mountInfo.mountPoint == mountPoint) {
            requestedDrives_.push_back(mountInfo);
            checkTime_ = system_clock::now();
            retval = true;
        }
    }
    return retval;
}

bool MountedDrives::check(const fs::path& path) const
{
    LOG_DEBUG(L"MountedDrives::check");
    for (auto i : mountedDrives_) {
        if (i.mountPoint == fsu::getMountRoot(path))
            return true;
    }
    return false;
}

#else
wstring volumeNameFromDriveletter(const wstring& driveletter)
{
	wstring volumeName(MAX_PATH, L'\0');
	DWORD volumeNameSize = static_cast<DWORD>(volumeName.size());
	if (GetVolumeInformationW(driveletter.c_str(), &volumeName[0], volumeNameSize, NULL, NULL, NULL, NULL, 0) == 0) {
		LOG_ERROR(L"Failed to get volume information");
		return L"";
	}
	volumeName.resize(wcslen(volumeName.c_str()));
	return volumeName;
}

bool MountedDrives::mountInfoExists(const MountInfo& mountInfo) const
{
    return std::find(requestedDrives_.begin(), requestedDrives_.end(), mountInfo) == requestedDrives_.end();
}

bool MountedDrives::refresh(const string& mountPoint, const std::wstring& name)
{
	LOG_DEBUG(L"MountedDrives::refresh");
    mountedDrives_.clear();
    checkTime_ = std::chrono::system_clock::now();

    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        LOG_ERROR(L"Failed to get logical drives");
        return false;
    }
    bool retval = false;
    char driveLetter = 'A';
    while (drives) {
        if (drives & 1) {
            std::wstring drive = std::wstring(1, driveLetter) + L":\\";
            UINT driveType = GetDriveType(drive.c_str());
            if (driveType != DRIVE_NO_ROOT_DIR) {
                MountInfo mountInfo;
                mountInfo.device = util::wstring_to_string(drive);
				mountInfo.mountPoint = mountInfo.device;

                switch (driveType) {
                case DRIVE_REMOVABLE:
                    mountInfo.fileSystemType = "Removable";
                    break;
                case DRIVE_FIXED:
                    mountInfo.fileSystemType = "Fixed";
                    break;
                case DRIVE_REMOTE:
                    mountInfo.fileSystemType = "Remote";
                    break;
                case DRIVE_CDROM:
                    mountInfo.fileSystemType = "CD-ROM";
                    break;
                case DRIVE_RAMDISK:
                    mountInfo.fileSystemType = "RAM Disk";
                    break;
                default:
                    mountInfo.fileSystemType = "Unknown";
                    break;
                }
                mountInfo.name = volumeNameFromDriveletter(drive);
                mountedDrives_.push_back(mountInfo);
                if (mountInfo.mountPoint == mountPoint && (name.empty() || mountInfo.name == name)) {
                    if (!mountInfoExists(mountInfo))
					    requestedDrives_.push_back(mountInfo);
					checkTime_ = system_clock::now();
					retval = true;
				}
            }
        }
        drives >>= 1;
        driveLetter++;
    }
	return retval;
}

bool MountedDrives::check(const fs::path& path) const
{
    LOG_DEBUG(L"MountedDrives::check");
    auto mp = fsu::getMountRoot(path);
    return mp.has_value();
    // for (auto i : mountedDrives_) {
    //     auto mp = getMountRoot(path);
    //     if (i.device == )
    //         return true;
    // }
    // return false;
}

#endif

bool MountedDrives::driveMounted(const fs::path& path, const std::wstring& name)
{
    LOG_DEBUG(L"MountedDrives::driveMounted");
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(_UNIX) && !defined(_MACOS)
    // A path under a known fstab mountpoint counts as mounted only while that
    // mountpoint is actually mounted. Without this, getMountRoot() walks up
    // past the missing mountpoint to "/" (always mounted) and an unplugged
    // drive reads as "file deleted" instead of "drive gone" - which
    // deleteInTarget acts on. Auto-mounted media without an fstab entry
    // cannot be told apart from an ordinary missing directory here.
    if (auto fstabMountpoint = fstabMountpointFor(path))
        return mountpointActive(*fstabMountpoint);
#endif
    if (timeElapsed()) {
        auto mp=fsu::getMountRoot(path);
        if (!mp)
            return false;
        string mountpoint = mp.value().string();
        return refresh(mountpoint, name);
    }
    else
        return check(path);
}

#ifdef _TEST
void MountedDrives::setTestTime(std::chrono::seconds duration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkTime_= system_clock::now() + duration;
}

bool MountedDrives::timeElapsedTest() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return timeElapsed();
}
#endif

} // namespace AFS_Core
