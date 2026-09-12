#include "pch.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "Translation.h"
#include <StrUtil.h>
#include <fstream>
#include <sstream>
#ifdef WIN32
    #include <Windows.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <unistd.h>
    #include  <cctype>
#endif

namespace AFS_Core {

using std::string;
using std::wstring;

namespace fs = std::filesystem;

namespace FilesystemUtil {

#ifdef _WIN32
    std::string home_dir()
    {
        const char* home_drive = getenv("HOMEDRIVE");
        const char* home_path = getenv("HOMEPATH");
        return home_drive && home_path ? std::string(home_drive) + home_path : std::string();
    }
#else
    std::string home_dir()
    {
        if (const char* homedir = getenv("HOME"))
            return homedir;
        const passwd* entry = getpwuid(getuid());
        return entry && entry->pw_dir ? entry->pw_dir : std::string();
    }
#endif

bool file_exists(const std::wstring& filename)
{
    std::error_code ec;
    return fs::exists(filename, ec);
}

wstring make_error_message(FileInfoResult result)
{
    switch (result) {
    case FileInfoResult::FileNotExist:
        return translation::translate("file_not_exist");
    case FileInfoResult::DriveNotMounted:
        return translation::translate("drive_not_mounted");
    case FileInfoResult::CannotReadFileAttributes:
        return translation::translate("file_attributes_unreadable");
    case FileInfoResult::InvalidFileTime:
        return translation::translate("file_time_invalid");
    case FileInfoResult::ErrorReadFileInfo:
        return translation::translate("file_info_read_error");
    default:
        return translation::translate("unknown_error");
    }
}

#ifdef WIN32

FileInfoResult fileinfo(const std::wstring& fname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    if (!file_exists(fname)) {
        fs::path file_path(fname);
        auto& mounted_drives = MountedDrives::instance();
        if (!mounted_drives.drive_mounted(file_path)) {
            return FileInfoResult::DriveNotMounted;
        }
        return FileInfoResult::FileNotExist;
    }

    _WIN32_FILE_ATTRIBUTE_DATA data{};
    BOOL rc = GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard, &data);
    if (!rc)
        return FileInfoResult::CannotReadFileAttributes;
    SYSTEMTIME st_utc;
    if (!FileTimeToSystemTime(&data.ftLastWriteTime, &st_utc))
        return FileInfoResult::InvalidFileTime;

    ULARGE_INTEGER ticks;
    ticks.LowPart = data.ftLastWriteTime.dwLowDateTime;
    ticks.HighPart = data.ftLastWriteTime.dwHighDateTime;
    last_write_time = fs::file_time_type(
        fs::file_time_type::duration(static_cast<int64_t>(ticks.QuadPart)));
        filesize = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return FileInfoResult::OK;
}
#else
FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    try {
        if (!fs::exists(fpathname)) {
            auto& mounted_drives = MountedDrives::instance();
            if (!mounted_drives.drive_mounted(fpathname)) {
                return FileInfoResult::DriveNotMounted;
            }
            return FileInfoResult::FileNotExist;
        }

        filesize = fs::file_size(fpathname);
        last_write_time = fs::last_write_time(fpathname);
        return FileInfoResult::OK;
    }
    catch (const std::exception&) {
        return FileInfoResult::ErrorReadFileInfo;
    }
}
#endif

bool touch(const wstring& fpathname)
{
    try {
        auto now = fs::file_time_type::clock::now();
        fs::last_write_time(fpathname, now);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        log_error(L"Error in function touch: ", e.what() );
        return false;
    }
}

static fs::path normalize_best_effort(const fs::path& p)
{
    std::error_code ec;
    fs::path n = fs::weakly_canonical(p, ec);
    return ec ? fs::path(p) : n;
}

static bool lstat_ok(const fs::path& p, struct stat& st)
{
#ifndef WIN32
    return ::lstat(p.c_str(), &st) == 0;
#else
    return ::stat(p.string().c_str(), &st) == 0;
#endif
}

#ifdef __linux__
static std::string decode_mountinfo_escapes(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size() &&
            std::isdigit(static_cast<unsigned char>(s[i+1])) &&
            std::isdigit(static_cast<unsigned char>(s[i+2])) &&
            std::isdigit(static_cast<unsigned char>(s[i+3]))) {
            int val = (s[i+1]-'0')*64 + (s[i+2]-'0')*8 + (s[i+3]-'0');
            out.push_back(static_cast<char>(val));
            i += 3;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::string without_trailing_slash(std::string s)
{
    if (s.size() > 1 && s.back() == '/') s.pop_back();
    return s.empty() ? "/" : s;
}

static std::optional<std::string> mountpoint_of_mountinfo_line(const std::string& line)
{
    std::istringstream iss(line);
    std::string field;
    for (int i = 0; i < 5; ++i)  // field 5 of /proc/self/mountinfo is the mount point with octal escapes
        if (!(iss >> field)) return std::nullopt;
    return without_trailing_slash(decode_mountinfo_escapes(field));
}

static bool mount_covers(const std::string& mountpoint, const std::string& target)
{
    return target.starts_with(mountpoint) &&
           (target.size() == mountpoint.size() || target[mountpoint.size()] == '/');
}

static std::optional<fs::path> nonroot_covering_mountpoint(const fs::path& target)
{
    std::ifstream mountinfo("/proc/self/mountinfo");
    if (!mountinfo) return std::nullopt;
    const std::string t = without_trailing_slash(normalize_best_effort(target).string());
    std::string deepest;  // the deepest covering mount is the longest one; "/" covers everything and is never reported
    for (std::string line; std::getline(mountinfo, line);) {
        auto mp = mountpoint_of_mountinfo_line(line);
        if (mp && *mp != "/" && mount_covers(*mp, t) && mp->size() > deepest.size()) deepest = *mp;
    }
    if (deepest.empty()) return std::nullopt;
    return fs::path(deepest);
}
#endif

std::optional<fs::path> is_mountpoint(const fs::path& path)
{
#ifdef __linux__
    return nonroot_covering_mountpoint(path);  // mountinfo is the most robust source on Linux; it also knows bind mounts
#else
    fs::path p = normalize_best_effort(path);  // fallback: device boundary between path and parent; misses bind mounts
    struct stat cur{}, par{};
    if (!lstat_ok(p, cur)) return std::nullopt;

    fs::path parent = p.has_parent_path() ? p.parent_path() : fs::path("/");
    if (!lstat_ok(parent, par)) return std::nullopt;

    if (cur.st_dev != par.st_dev)
        return p;
    return std::nullopt;
#endif
}

std::optional<fs::path> get_mount_root(const fs::path& in)
{
#ifdef WIN32
    if (in.has_root_name()) {  // 'in' is used unnormalized: normalize_best_effort may return UNC paths
        fs::path root = in.root_name();
        root /= "\\";
        return root;
    }
    std::wstring path_str = in.wstring();  // [FilesystemUtil.md#unix-style-paths-on-windows]
    if (!path_str.empty() && path_str[0] == L'/') {
        return fs::path("/");
    }
    return std::nullopt;
#else
    fs::path current = normalize_best_effort(in);

    struct stat st_cur{};  // climb up until something exists
    while (!lstat_ok(current, st_cur)) {
        if (!current.has_parent_path()) return std::nullopt;
        current = current.parent_path();
    }

    for (;;) {
        fs::path parent = current.has_parent_path() ? current.parent_path() : fs::path("/");
        struct stat st_par{};
        if (!lstat_ok(parent, st_par)) return std::nullopt;

        if (st_cur.st_dev != st_par.st_dev) {
            return current.empty() ? fs::path("/") : current;  // the device changes here, so 'current' is the mount point
        }

        if (current == "/") {
            return fs::path("/");
        }

        current = parent;
        st_cur = st_par;
    }
#endif
}

} // namespace FilesystemUtil

} // namespace AFS_Core
