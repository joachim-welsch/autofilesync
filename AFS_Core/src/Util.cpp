#include "pch.h"
#include "Util.h"
#include "Translation.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <ctime>
#include <memory>
#include <regex>
#include <stdexcept>
#ifdef _WIN32
#  include <process.h>   // _getpid
#else
#  include <unistd.h>    // getpid
#endif
#ifdef _WXWIDGETS
    #include "wx/msgdlg.h"
    #include "wx/string.h"
#else
    #ifndef _UNIX
        #include <Windows.h>
        #include <tchar.h>
    #endif
#endif

namespace AFS_Core {

using std::string;
using std::wstring;

namespace fs=std::filesystem;

namespace util {

unsigned long current_pid()
{
#ifdef _WIN32
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

uint16_t to_uint16_t(const std::wstring& s)
{
    const int value = std::stoi(s);
    if (value < 0 || value > std::numeric_limits<uint16_t>::max())
        throw std::out_of_range(translation::translate_utf8("value_out_of_uint16_range") + wstring_to_string(s));
    return static_cast<uint16_t>(value);
}

std::wstring with_trailing_char(const std::wstring& s, wchar_t c)
{
    if (!s.empty() && s.back() == c)  // [Util.md#with_trailing_char-empty-input]
        return s;
    return s + c;
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
#ifdef _UNIX
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

static std::tm local_time(std::time_t tt)
{
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

static std::tm utc_time(std::time_t tt)
{
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    return tm;
}

static std::wstring timestamp(std::chrono::system_clock::time_point time)  // [Util.md#timestamp]
{
    using namespace std::chrono;
    const auto second = floor<seconds>(time);
    const auto millis = duration_cast<milliseconds>(time - second).count();
    const std::tm tm = local_time(system_clock::to_time_t(second));
    std::wostringstream os;
    os << std::put_time(&tm, L"%Y-%m-%d_%H-%M-%S") << L'-'
       << std::setw(3) << std::setfill(L'0') << millis;
    return os.str();
}

fs::path with_timestamp_prefix(const fs::path& pathname, std::chrono::system_clock::time_point time)
{
    if (pathname.filename().empty())  // [Util.md#filename-required]
        return pathname;
    return pathname.parent_path() / (timestamp(time) + L"_" + pathname.filename().wstring());
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

static std::wregex wildcard_to_regex(const std::wstring& wildcard, bool case_sensitive = true)
{
    std::wstring regex;  // [Util.md#wildcard-escaping]
    regex.reserve(wildcard.size() * 2);
    for (wchar_t c : wildcard) {
        switch (c) {
        case L'*': regex += L".*"; break;
        case L'?': regex += L'.'; break;
        case L'.': case L'^': case L'$': case L'|': case L'(': case L')':
        case L'[': case L']': case L'{': case L'}': case L'+': case L'\\':
            regex += L'\\'; regex += c; break;
        default: regex += c;
        }
    }
    return std::wregex(regex, case_sensitive ? std::regex_constants::ECMAScript : std::regex_constants::icase);
}

bool wildmatch(const std::wstring& input, const std::wstring& wildcard)
{
    return std::regex_match(input, wildcard_to_regex(wildcard));
}

namespace {
    using FilePtr = std::unique_ptr<FILE, int(*)(FILE*)>;

    FilePtr open_for_reading(const wstring& name)
    {
        FILE* file = nullptr;
#ifdef WIN32
        _wfopen_s(&file, name.c_str(), L"rb");
#else
        file = std::fopen(wstring_to_string(name).c_str(), "rb");
#endif
        return FilePtr(file, std::fclose);
    }
}

bool files_equal(const wstring& input1, const wstring& input2)
{
    const FilePtr f1 = open_for_reading(input1);
    const FilePtr f2 = open_for_reading(input2);
    if (!f1 || !f2)
        return false;
    constexpr size_t N = 4096;
    char buf1[N];
    char buf2[N];
    do {
        const size_t r1 = std::fread(buf1, 1, N, f1.get());
        const size_t r2 = std::fread(buf2, 1, N, f2.get());
        if (r1 != r2 || std::memcmp(buf1, buf2, r1) != 0)
            return false;
    } while (!std::feof(f1.get()) || !std::feof(f2.get()));
    return std::feof(f1.get()) && std::feof(f2.get());
}

std::time_t to_time_t(const fs::file_time_type& ft)  // [Util.md#file-time-conversions]
{
    using namespace std::chrono;
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++
    const auto sys = file_clock::to_sys(ft);
#else
    const auto sys = clock_cast<system_clock>(ft);
#endif
    return system_clock::to_time_t(time_point_cast<system_clock::duration>(sys));
}

fs::file_time_type to_file_time(std::time_t tt)  // [Util.md#file-time-conversions]
{
    using namespace std::chrono;
    const auto sys = system_clock::from_time_t(tt);
#ifdef _MACOS
    return time_point_cast<fs::file_time_type::duration>(file_clock::from_sys(sys));
#else
    return time_point_cast<fs::file_time_type::duration>(clock_cast<file_clock>(sys));
#endif
}

string to_string(const fs::file_time_type& ft)
{
    const std::tm tm = utc_time(to_time_t(ft));
    std::stringstream buffer;
    buffer << std::put_time(&tm, "%A, %d %B %Y %H:%M");
    return buffer.str();
}

bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++, so file_clock::to_sys is used
    return std::chrono::file_clock::to_sys(a) == std::chrono::file_clock::to_sys(b);
#else
    return a == b;
#endif
}

bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++, so file_clock::to_sys is used
    return std::chrono::file_clock::to_sys(a) > std::chrono::file_clock::to_sys(b);
#else
    return a > b;
#endif
}

bool rename_file(const std::wstring& oldname, const std::wstring& newname)
{
    log_info(L"rename_file", oldname, newname);
    try {
        fs::rename(oldname, newname);
        return true;
    }
    catch (const fs::filesystem_error& ex) {
        log_error(translation::translate("file_rename_error") + string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(translation::translate("file_rename_error"), oldname, translation::translate("unknown_error"));
    }
    return false;
}

bool remove_file(const std::wstring& name, LogLevel ll)
{
    try {
        return fs::remove(name);  // [Util.md#remove-returns-false]
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

} // namespace util

} // namespace AFS_Core
