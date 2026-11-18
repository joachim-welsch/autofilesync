/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include "pch.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "Translation.h"
#include <StrUtil.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#ifdef _MACOS
    #include <sys/param.h>
    #include <sys/mount.h>
#endif
#ifdef _WXWIDGETS
    #include "wx/msgdlg.h"
    #include "wx/string.h"
#elif !defined(_UNIX)
    #include <tchar.h>
#endif
#ifdef WIN32
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <unistd.h>
    #include  <cctype>
#endif

namespace Utilities {

using std::string;
using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

namespace fs = std::filesystem;

namespace FilesystemUtil {

#ifdef _WIN32
    std::string home_dir()  // [FilesystemUtil.md#home_dir-windows]
    {
        if (const char* profile = getenv("USERPROFILE"))
            return profile;
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

std::optional<fs::path> env_path(const char* name)  // [FilesystemUtil.md#env_path]
{
#ifdef _WIN32
    const std::wstring wide_name(name, name + std::char_traits<char>::length(name));
    const wchar_t* value = _wgetenv(wide_name.c_str());
#else
    const char* value = getenv(name);
#endif
    if (value == nullptr || *value == 0)
        return std::nullopt;
    return fs::path(value);
}

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
    case FileInfoResult::DirectoryNotExist:
        return translation::translate("directory_not_exist");
    case FileInfoResult::DriveNotMounted:
        return translation::translate("drive_not_mounted");
    case FileInfoResult::CannotReadFileAttributes:
        return translation::translate("file_attributes_unreadable");
    case FileInfoResult::CannotSetFileAttributes:
        return translation::translate("file_attributes_not_settable");
    case FileInfoResult::PermissionDenied:
        return translation::translate("permission_denied");
    case FileInfoResult::FileLocked:
        return translation::translate("file_locked");
    case FileInfoResult::InvalidFileTime:
        return translation::translate("file_time_invalid");
    case FileInfoResult::ErrorReadFileInfo:
        return translation::translate("file_info_read_error");
    case FileInfoResult::ErrorWriteFileInfo:
        return translation::translate("file_info_write_error");
    case FileInfoResult::CannotCreateDirectory:
        return translation::translate("directory_not_creatable");
    case FileInfoResult::DiskFull:
        return translation::translate("disk_full");
    case FileInfoResult::LinksNotSupported:
        return translation::translate("links_not_supported");
    case FileInfoResult::ServerUnreachable:
        return translation::translate("server_connect_failed");
    case FileInfoResult::ReadOnlyFilesystem:
        return translation::translate("filesystem_read_only");
    case FileInfoResult::RecycleBinFull:
        return translation::translate("recycle_bin_full");
    default:
        return translation::translate("unknown_error");
    }
}

static bool lock_error(const std::error_code& ec)  // [FilesystemUtil.md#lock_error]
{
#ifdef WIN32
    return ec.category() == std::system_category() && (ec.value() == ERROR_SHARING_VIOLATION || ec.value() == ERROR_LOCK_VIOLATION);
#else
    return ec == std::errc::text_file_busy || ec == std::errc::device_or_resource_busy;
#endif
}

static bool space_error(const std::error_code& ec)  // [FilesystemUtil.md#space_error]
{
#ifdef WIN32
    if (ec.category() == std::system_category() && (ec.value() == ERROR_DISK_FULL || ec.value() == ERROR_HANDLE_DISK_FULL || ec.value() == ERROR_DISK_QUOTA_EXCEEDED))
        return true;
#else
    if (ec == std::error_code(EDQUOT, std::generic_category()))
        return true;
#endif
    return ec == std::errc::no_space_on_device;
}

static bool read_only_error(const std::error_code& ec)  // [FilesystemUtil.md#read_only_error]
{
#ifdef WIN32
    if (ec.category() == std::system_category() && ec.value() == ERROR_WRITE_PROTECT)
        return true;
#endif
    return ec == std::errc::read_only_file_system;
}

FileInfoResult reason_for_failure(const std::error_code& ec)  // [FilesystemUtil.md#reason_for_failure]
{
    if (lock_error(ec))
        return FileInfoResult::FileLocked;
    if (read_only_error(ec))  // [FilesystemUtil.md#read_only_error] before the permission check
        return FileInfoResult::ReadOnlyFilesystem;
    if (space_error(ec))
        return FileInfoResult::DiskFull;
    if (ec == std::errc::permission_denied)
        return FileInfoResult::PermissionDenied;
    return ec == std::errc::no_such_file_or_directory ? FileInfoResult::FileNotExist : FileInfoResult::ErrorReadFileInfo;
}

FileInfoResult reason_for_write_failure(const std::error_code& ec)  // [FilesystemUtil.md#reason_for_write_failure]
{
    const FileInfoResult reason = reason_for_failure(ec);
    return reason == FileInfoResult::ErrorReadFileInfo ? FileInfoResult::ErrorWriteFileInfo : reason;
}

std::wstring failure_text(const std::error_code& ec)  // [FilesystemUtil.md#failure_text]
{
    const FileInfoResult reason = reason_for_failure(ec);
    if (reason != FileInfoResult::ErrorReadFileInfo)
        return make_error_message(reason);
    return ec ? string_to_wstring(ec.message()) : translation::translate("unknown_error");
}

FileInfoResult create_directory_refusal(const fs::path& directory, std::error_code& ec)  // [FilesystemUtil.md#create_directory_refusal]
{
    fs::create_directories(directory, ec);
    if (!ec && fs::is_directory(directory, ec))
        return FileInfoResult::OK;
    if (!ec)
        ec = std::make_error_code(std::errc::not_a_directory);
    const FileInfoResult reason = reason_for_failure(ec);
    const bool told = reason == FileInfoResult::PermissionDenied || reason == FileInfoResult::FileLocked || reason == FileInfoResult::DiskFull
        || reason == FileInfoResult::ReadOnlyFilesystem;
    return told ? reason : FileInfoResult::CannotCreateDirectory;
}

#ifdef _TEST
static std::optional<uint64_t> free_space_for_test;

void set_free_space_for_test(std::optional<uint64_t> bytes)  // [FilesystemUtil.md#free_space]
{
    free_space_for_test = bytes;
}
#endif

std::optional<uint64_t> free_space(const fs::path& directory)  // [FilesystemUtil.md#free_space]
{
#ifdef _TEST
    if (free_space_for_test)
        return free_space_for_test;
#endif
    std::error_code ec;
    const fs::space_info space = fs::space(directory, ec);
    return ec || space.capacity == 0 ? std::nullopt : std::optional<uint64_t>(space.available);  // [FilesystemUtil.md#free_space] a drive without a size does not say
}

static fs::path existing_part(fs::path path)  // [FilesystemUtil.md#read_only_filesystem]
{
    std::error_code ec;
    while (!fs::exists(path, ec) && path.has_relative_path() && path != path.parent_path())
        path = path.parent_path();
    return path;
}

#ifdef _TEST
static std::optional<fs::path> read_only_root_for_test;

void set_read_only_filesystem_for_test(std::optional<fs::path> root)  // [FilesystemUtil.md#read_only_filesystem]
{
    read_only_root_for_test = std::move(root);
}

static bool below_read_only_root_for_test(const fs::path& path)
{
    if (!read_only_root_for_test)
        return false;
    const fs::path relative = path.lexically_normal().lexically_relative(read_only_root_for_test->lexically_normal());
    return !relative.empty() && *relative.begin() != "..";
}
#endif

bool read_only_filesystem(const fs::path& path)  // [FilesystemUtil.md#read_only_filesystem]
{
#ifdef _TEST
    if (below_read_only_root_for_test(path))
        return true;
#endif
    const fs::path existing = existing_part(path);
#ifdef WIN32
    wchar_t volume[MAX_PATH + 1] = {};
    DWORD flags = 0;
    return GetVolumePathNameW(existing.c_str(), volume, MAX_PATH + 1) && GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0)
        && (flags & FILE_READ_ONLY_VOLUME) != 0;
#else
    struct statvfs volume {};
    return statvfs(existing.c_str(), &volume) == 0 && (volume.f_flag & ST_RDONLY) != 0;
#endif
}

std::error_code last_stream_error()  // [FilesystemUtil.md#last_stream_error]
{
#ifdef WIN32
    return std::error_code(static_cast<int>(_doserrno), std::system_category());
#else
    return std::error_code(errno, std::generic_category());
#endif
}

static FileInfoResult reason_for_missing(const fs::path& file)  // [FilesystemUtil.md#reason_for_missing]
{
    if (!MountedDrives::instance().drive_mounted(file))
        return FileInfoResult::DriveNotMounted;
    const fs::path directory = file.parent_path();
    std::error_code ec;
    if (directory.empty() || fs::is_directory(directory, ec))
        return FileInfoResult::FileNotExist;
    return ec == std::errc::permission_denied ? FileInfoResult::PermissionDenied : FileInfoResult::DirectoryNotExist;
}

#ifdef WIN32

FileInfoResult fileinfo(const std::wstring& fname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    std::error_code ec;
    if (fs::status(fname, ec).type() == fs::file_type::not_found)
        return reason_for_missing(fname);
    if (ec)
        return reason_for_failure(ec);

    _WIN32_FILE_ATTRIBUTE_DATA data{};
    BOOL rc = GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard, &data);
    if (!rc) {
        const FileInfoResult reason = reason_for_failure(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
        return reason == FileInfoResult::ErrorReadFileInfo ? FileInfoResult::CannotReadFileAttributes : reason;
    }
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
    std::error_code ec;
    if (fs::status(fpathname, ec).type() == fs::file_type::not_found)
        return reason_for_missing(fpathname);
    if (ec)
        return reason_for_failure(ec);
    try {
        filesize = fs::file_size(fpathname);
        last_write_time = fs::last_write_time(fpathname);
        return FileInfoResult::OK;
    }
    catch (const fs::filesystem_error& e) {
        return reason_for_failure(e.code());
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
        if (!lstat_ok(parent, st_par))
            return std::nullopt;

        if (st_cur.st_dev != st_par.st_dev)
            return current.empty() ? fs::path("/") : current;  // the device changes here, so 'current' is the mount point
        if (current == "/") {
            return fs::path("/");
        }
        current = parent;
        st_cur = st_par;
    }
#endif
}

bool plain_entry_name(std::string_view name)  // [FilesystemUtil.md#plain_entry_name]
{
    constexpr std::string_view SEPARATORS("/\\\0", 3);
    return !name.empty() && name != "." && name != ".." && name.find_first_of(SEPARATORS) == std::string_view::npos;
}

#if !defined(_WXWIDGETS) && defined(_UNIX)
bool drive_exists(const wstring& drive_name)
{
    log_debug(L"drive_exists() " + drive_name);
    std::error_code ec;
    return fs::exists(drive_name, ec);
}
#endif

#ifdef _WXWIDGETS
bool drive_exists(const wstring& drive_name)
{
    log_debug(L"drive_exists() " + drive_name);
    #ifdef _UNIX
    if (drive_name.empty())
        return false;
    #endif
    return wxDirExists(drive_name);
}
#elif defined(WIN32)
bool drive_exists(const wstring& drive_name)
{
    log_debug(L"drive_exists() " + drive_name);
    std::error_code ec;
    return fs::is_directory(drive_name, ec);
}
#endif

bool volume_exists(const wstring& drive_name, const wstring& volume_name)
{
    log_debug(L"volume_exists() " + drive_name + L" " + volume_name);
#ifdef _MACOS
    const string volume = wstring_to_string(volume_name);
    struct statfs* mounts = nullptr;
    const int count = getmntinfo(&mounts, MNT_NOWAIT);
    const auto names_volume = [&](const char* name) { return string(name).find(volume) != string::npos; };
    for (int i = 0; i < count; ++i)
        if (names_volume(mounts[i].f_mntfromname) || names_volume(mounts[i].f_mntonname))
            return true;
    return false;
#elif defined(_UNIX)
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        log_error(translation::translate("proc_mounts_not_openable"));
        return false;
    }
    const string volume = wstring_to_string(volume_name);
    std::string line;
    while (std::getline(mounts, line))
        if (line.find(volume) != std::string::npos)
            return true;
    return false;
#else
    TCHAR fetched_volume_name[MAX_PATH + 1] = { 0 };
    TCHAR file_system_name[MAX_PATH + 1] = { 0 };
    DWORD serial_number = 0;
    DWORD max_component_length = 0;
    DWORD file_system_flags = 0;
    if (!GetVolumeInformation(drive_name.c_str(), fetched_volume_name, ARRAYSIZE(fetched_volume_name),
            &serial_number, &max_component_length, &file_system_flags, file_system_name, ARRAYSIZE(file_system_name)))
        return false;
    return _tcslen(fetched_volume_name) > 0 && volume_name == fetched_volume_name;
#endif
}

void handle_last_error(std::filesystem::filesystem_error const& ex)
{
    std::stringstream msg;
    msg << "what():  " << ex.what() << ' '
        << "path1(): " << ex.path1() << ' '
        << "path2(): " << ex.path2() << ' '
        << "code().value():    " << ex.code().value() << ' '
        << "code().message():  " << ex.code().message() << ' '
        << "code().category(): " << ex.code().category().name() << ' ';
    log_error(msg.str());
}

namespace {
    using FilePtr = std::unique_ptr<FILE, int(*)(FILE*)>;

    FilePtr open_for_reading(const wstring& name, std::error_code& ec)
    {
        FILE* file = nullptr;
#ifdef WIN32
        _wfopen_s(&file, name.c_str(), L"rb");
#else
        file = std::fopen(wstring_to_string(name).c_str(), "rb");
#endif
        if (file == nullptr)
            ec = last_stream_error();
        return FilePtr(file, std::fclose);
    }

    bool same_block(FILE* file1, FILE* file2)
    {
        constexpr size_t BLOCK_BYTES = 4096;
        char block1[BLOCK_BYTES];
        char block2[BLOCK_BYTES];
        const size_t read1 = std::fread(block1, 1, BLOCK_BYTES, file1);
        const size_t read2 = std::fread(block2, 1, BLOCK_BYTES, file2);
        return read1 == read2 && std::memcmp(block1, block2, read1) == 0;
    }
}

FileInfoResult read_refusal(const wstring& file)  // [FilesystemUtil.md#read_refusal]
{
    std::error_code ec;
    return open_for_reading(file, ec) ? FileInfoResult::OK : reason_for_failure(ec);
}

ContentComparison compare_content(const wstring& input1, const wstring& input2)  // [FilesystemUtil.md#compare_content]
{
    std::error_code ec1, ec2;
    const FilePtr f1 = open_for_reading(input1, ec1);
    const FilePtr f2 = open_for_reading(input2, ec2);
    if (!f1)
        return {false, reason_for_failure(ec1), input1};
    if (!f2)
        return {false, reason_for_failure(ec2), input2};
    bool same = true;
    while (same && !(std::feof(f1.get()) && std::feof(f2.get())))
        same = same_block(f1.get(), f2.get());
    if (std::ferror(f1.get()) || std::ferror(f2.get()))
        return {false, reason_for_failure(last_stream_error()), std::ferror(f1.get()) ? input1 : input2};
    return {same, FileInfoResult::OK, {}};
}

bool files_equal(const wstring& input1, const wstring& input2)
{
    return compare_content(input1, input2).equal;
}

#ifdef _TEST
static std::optional<std::error_code> rename_error_for_test;
static int renames_before_error_for_test = 0;
static int rename_errors_left_for_test = 0;

void set_rename_error_for_test(std::optional<std::error_code> error, int succeeding_first, int failing)  // [FilesystemUtil.md#rename_refusal]
{
    rename_error_for_test = error;
    renames_before_error_for_test = succeeding_first;
    rename_errors_left_for_test = failing;
}

static bool rename_fails_for_test()
{
    if (!rename_error_for_test || renames_before_error_for_test-- > 0)
        return false;
    return rename_errors_left_for_test-- > 0;
}
#endif

FileInfoResult rename_refusal(const std::wstring& oldname, const std::wstring& newname)  // [FilesystemUtil.md#rename_refusal]
{
    log_info(L"rename_file", oldname, newname);
#ifdef _TEST
    if (rename_fails_for_test())
        return reason_for_write_failure(*rename_error_for_test);
#endif
    std::error_code ec;
    fs::rename(oldname, newname, ec);
    if (!ec)
        return FileInfoResult::OK;
    log_error(translation::translate("file_rename_error") + oldname + L" -> " + newname + L": " + string_to_wstring(ec.message()));
    return reason_for_write_failure(ec);
}

bool rename_file(const std::wstring& oldname, const std::wstring& newname)
{
    return rename_refusal(oldname, newname) == FileInfoResult::OK;
}

bool remove_file(const std::wstring& name, LogLevel ll)
{
    try {
        return fs::remove(name);  // [FilesystemUtil.md#remove-returns-false]
    }
    catch (const fs::filesystem_error& ex) {
        if (ll != LogLevel::None)
            Logging::instance().log(ll, L"Error removing file: " + string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(L"remove_file: ", name, translation::translate("unknown_error"));
    }
    return false;
}

FileInfoResult remove_refusal(const std::wstring& name)  // [FilesystemUtil.md#remove_refusal]
{
    std::error_code ec;
    if (fs::remove(name, ec))
        return FileInfoResult::OK;
    return ec ? reason_for_write_failure(ec) : FileInfoResult::FileNotExist;  // [FilesystemUtil.md#remove-returns-false]
}

bool set_last_write_time(const std::wstring& fn, const fs::file_time_type& modify_time, LogLevel ll)
{
    try {
        fs::last_write_time(fn, modify_time);
        return true;
    }
    catch (const fs::filesystem_error& ex) {
        if (ll != LogLevel::None)
            Logging::instance().log(ll, L"Error setting last write time: " + string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(L"set_last_write_time: ", fn, translation::translate("unknown_error"));
    }
    return false;
}

#ifdef _TEST
static std::optional<std::error_code> write_time_error_for_test;

void set_write_time_error_for_test(std::optional<std::error_code> error)  // [FilesystemUtil.md#write_time_refusal]
{
    write_time_error_for_test = error;
}
#endif

FileInfoResult write_time_refusal(const std::wstring& file, const fs::file_time_type& time)  // [FilesystemUtil.md#write_time_refusal]
{
#ifdef _TEST
    if (write_time_error_for_test)
        return reason_for_write_failure(*write_time_error_for_test);
#endif
    std::error_code ec;
    fs::last_write_time(file, time, ec);
    return ec ? reason_for_write_failure(ec) : FileInfoResult::OK;
}

#ifdef _TEST
static std::optional<std::error_code> permissions_error_for_test;

void set_permissions_error_for_test(std::optional<std::error_code> error)  // [FilesystemUtil.md#permissions_refusal]
{
    permissions_error_for_test = error;
}
#endif

static bool rights_unknown_to_filesystem(const std::error_code& ec)  // [FilesystemUtil.md#permissions_refusal]
{
    return ec == std::errc::not_supported || ec == std::errc::operation_not_supported || ec == std::errc::function_not_supported;
}

static FileInfoResult reason_for_permissions_failure(const std::error_code& ec)  // [FilesystemUtil.md#permissions_refusal]
{
    if (!ec || rights_unknown_to_filesystem(ec))
        return FileInfoResult::OK;
    const FileInfoResult reason = reason_for_write_failure(ec);
    return reason == FileInfoResult::ErrorWriteFileInfo ? FileInfoResult::CannotSetFileAttributes : reason;
}

FileInfoResult permissions_refusal(const std::wstring& file, fs::perms permissions)  // [FilesystemUtil.md#permissions_refusal]
{
#ifdef _TEST
    if (permissions_error_for_test)
        return reason_for_permissions_failure(*permissions_error_for_test);
#endif
    std::error_code ec;
    fs::permissions(file, permissions, ec);
    return reason_for_permissions_failure(ec);
}

std::wstring read_utf8_file(const fs::path& file)
{
    std::ifstream in(file);
    if (!in)
        throw std::ios_base::failure("cannot open " + file.string());
    std::ostringstream bytes;
    bytes << in.rdbuf();
    return string_to_wstring(bytes.str());
}

void write_utf8_file(const fs::path& file, const std::wstring& text)
{
    const std::string bytes = wstring_to_string(text);
    if (bytes.empty() && !text.empty())
        throw std::runtime_error("text not convertible to UTF-8: " + file.string());  // [FilesystemUtil.md#utf8-files]
    std::ofstream os(file, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);  // [FilesystemUtil.md#utf8-line-ends]
    os.exceptions(std::ios::failbit | std::ios::badbit);
    os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    os.close();  // [FilesystemUtil.md#utf8-written] a short text only reaches the disk here, and the destructor would keep quiet about it
}

bool write_private_file(const fs::path& file, std::string_view bytes)  // [FilesystemUtil.md#private-files]
{
#ifdef _WIN32
    std::ofstream os(file, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    os.close();  // [FilesystemUtil.md#utf8-written]
    return static_cast<bool>(os);
#else
    const int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    const bool written = fchmod(fd, 0600) == 0 && write(fd, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size());
    return close(fd) == 0 && written;
#endif
}

} // namespace FilesystemUtil

} // namespace Utilities
