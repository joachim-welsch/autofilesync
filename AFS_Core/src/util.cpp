#include "pch.h"
#include "util.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <regex>
#include <locale>
#include <codecvt>
#include <sys/stat.h>
#include <sys/types.h>
#include  <cctype>
#ifdef _WIN32
#  include <process.h>   // _getpid
#else
#  include <unistd.h>    // getpid
#endif
#ifdef _WXWIDGETS
    #include "wx/msgdlg.h"
    #include "wx/string.h"
    #ifdef _UNIX
        #include <pwd.h>
    #endif
#else
    #ifdef _UNIX
        #include <unistd.h>
        #include <pwd.h>
        #include <string.h>
    #else
        #include <Windows.h>
        #include <tchar.h>
    #endif
#endif

namespace AFS_Core {

using std::string;
using std::wstring;

namespace fs=std::filesystem;

namespace util {

unsigned long currentPid()
    {
#ifdef _WIN32
        return static_cast<unsigned long>(::_getpid());
#else
        return static_cast<unsigned long>(::getpid());
#endif
    }

std::wstring withTrailingChar(const std::wstring& s, wchar_t c)
{
    // s.empty() must be checked first: *rbegin() on an empty string is UB
    // (hard iterator-debug assert on MSVC). Empty input keeps the contract
    // "result ends with c".
    if (!s.empty() && s.back() == c)
        return s;
    return s + c;
}

#if !defined(_WXWIDGETS) && defined(_UNIX)
bool driveExists(const wstring& driveName)
{
    LOG_DEBUG(L"driveExists() " + driveName);
    try {
        return fs::exists(driveName);
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR(L"Fehler beim Zugriff auf das Dateisystem: " + string_to_wstring(e.what()));
        return false;
    }
}
#endif

#ifdef _WXWIDGETS
bool driveExists(const wstring& driveName)
{
    LOG_DEBUG(L"driveExists() " + driveName);
    #ifdef _UNIX
    if (driveName.empty())
        return false;
    #endif
    return wxDirExists(driveName);
}
#elif defined(WIN32)
bool driveExists(const wstring& driveName)
{
	LOG_DEBUG(L"driveExists() " + driveName);
    return std::filesystem::exists(driveName) && std::filesystem::is_directory(driveName);
}

#endif

bool volumeExists(const wstring& driveName, const wstring& volumeName)
{
    LOG_DEBUG(L"volumeExists() " + driveName + L" " + volumeName);
#ifdef _UNIX
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        std::cerr << "Fehler beim Öffnen von /proc/mounts" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(wstring_to_string(volumeName)) != std::string::npos) {
            return true;
        }
    }
    return false;
#else
    TCHAR fetchedVolumeName[MAX_PATH + 1] = { 0 };
    TCHAR fileSystemName[MAX_PATH + 1] = { 0 };
    DWORD serialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;
    if (GetVolumeInformation(
        driveName.c_str(),
        fetchedVolumeName,
        ARRAYSIZE(fetchedVolumeName),
        &serialNumber,
        &maxComponentLength,
        &fileSystemFlags,
        fileSystemName,
        ARRAYSIZE(fileSystemName)
    )) {
        return _tcslen(fetchedVolumeName) > 0 && volumeName == fetchedVolumeName;
    }
    else {
        return false;
    }
#endif
}


// floor() instead of system_clock::to_time_t(): to_time_t may round to the
// nearest second, which would pair a rounded-up second with the milliseconds of
// the original one and yield a timestamp that never existed.
static std::wstring timestamp(std::chrono::system_clock::time_point time)
{
    using namespace std::chrono;
    const auto second = floor<seconds>(time);
    const auto millis = duration_cast<milliseconds>(time - second).count();
    const std::time_t tt = system_clock::to_time_t(second);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::wostringstream os;
    os << std::put_time(&tm, L"%Y-%m-%d_%H-%M-%S") << L'-'
       << std::setw(3) << std::setfill(L'0') << millis;
    return os.str();
}

fs::path withTimestampPrefix(const fs::path& pathname, std::chrono::system_clock::time_point time)
{
    // A path ending in a separator (or an empty one) has no file name to
    // prefix; prefixing the last directory instead would silently rename it.
    if (pathname.filename().empty())
        return pathname;
    return pathname.parent_path() / (timestamp(time) + L"_" + pathname.filename().wstring());
}

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
              + system_clock::now());
    return system_clock::to_time_t(sctp);
}
void handleLastError(std::filesystem::filesystem_error const& ex)
{
    std::stringstream msg;
    msg << "what():  " << ex.what() << ' '
        << "path1(): " << ex.path1() << ' '
        << "path2(): " << ex.path2() << ' '
        << "code().value():    " << ex.code().value() << ' '
        << "code().message():  " << ex.code().message() << ' '
        << "code().category(): " << ex.code().category().name() << ' ';
    LOG_ERROR(msg.str())
}

std::wregex wildcardToRegex(const std::wstring& wildcard, bool caseSensitive = true) {
    // Everything except the wildcard metacharacters * and ? must be escaped:
    // an unescaped '.' made "*.txt" match "abXtxt", parentheses turned into
    // regex groups so "Fotos (2020)*" never matched its own files, and an
    // unbalanced '[' or '(' threw std::regex_error on every comparison.
    std::wstring regex;
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
    return std::wregex(regex, caseSensitive ? std::regex_constants::ECMAScript : std::regex_constants::icase);
}

bool wildmatch(const std::wstring& input, const std::wstring& wildcard)
{
    auto rgx = wildcardToRegex(wildcard);
    return std::regex_match(input, rgx);
}

#ifdef WIN32
bool filesEqual(const wstring& file1, const wstring& file2)
{
    FILE* fp1;
    FILE* fp2;
    errno_t err = _wfopen_s(&fp1, file1.c_str(), L"rb");
    if (err != 0)
        return false;
    err = _wfopen_s(&fp2, file2.c_str(), L"rb");
    if (err != 0)
        return false;
    if ((fp1 == nullptr) || (fp2 == nullptr))
        return false;
    const int N = 4096;
    char buf1[N];
    char buf2[N];
    do {
        size_t r1 = fread(buf1, 1, N, fp1);
        size_t r2 = fread(buf2, 1, N, fp2);
        if (r1 != r2 || memcmp(buf1, buf2, r1)) {
            fclose(fp1);
            fclose(fp2);
            return false;
        }
    } while (!feof(fp1) || !feof(fp2));
    const bool retval = feof(fp1) && feof(fp2);
    fclose(fp1);
    fclose(fp2);
    return retval;
}
#else
bool filesEqual(const wstring& input1, const wstring& input2) {
  constexpr int N = 4096;
  char buf1[N];
  char buf2[N];
  FILE* f1 = fopen(wstring_to_string(input1).c_str(), "rb");
  FILE* f2 = fopen(wstring_to_string(input2).c_str(), "rb");
  if (!f1 || !f2) {
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    return false;
  }
  do {
    size_t r1 = fread(buf1, 1, N, f1);
    size_t r2 = fread(buf2, 1, N, f2);
    if (r1 != r2 || memcmp(buf1, buf2, r1)) {
      fclose(f1);
      fclose(f2);
      return false;
    }
  } while (!feof(f1) || !feof(f2));
  bool retval = feof(f1) && feof(f2);
  fclose(f1);
  fclose(f2);
  return retval;
}
#endif

string toString(const fs::file_time_type& ft)
{
    std::time_t tt = to_time_t(ft);
    std::tm *gmt = std::gmtime(&tt);
    std::stringstream buffer;
    buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
    return buffer.str();
}

bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS
    // clock_cast is unavailable in Apple's libc++, so use file_clock::to_sys instead
    auto a_sys = std::chrono::file_clock::to_sys(a);
    auto b_sys = std::chrono::file_clock::to_sys(b);
    return a_sys == b_sys;
#endif
    return a == b;
}

bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS
    // clock_cast is unavailable in Apple's libc++, so use file_clock::to_sys instead
    auto a_sys = std::chrono::file_clock::to_sys(a);
    auto b_sys = std::chrono::file_clock::to_sys(b);
    return a_sys > b_sys;
#endif
	return a > b;
}

bool rename_file(const std::wstring& oldname, const std::wstring& newname)
{
    LOG_INFO3(L"rename_file", oldname, newname);
    try
    {
        fs::rename(oldname, newname);
    }
    catch (const fs::filesystem_error& ex) //// TODO use handleLastError
    {
		LOG_ERROR(L"Fehler beim Umbenennen der Datei: " + string_to_wstring(ex.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR3(L"Fehler beim Umbenennen der Datei: ", oldname, L"Unbekannter Fehler")
    }
    return true;
}

bool remove_file(const std::wstring& name, LogLevel ll)
{
	try	{
		// fs::remove()'s own return value (false = nothing there to remove,
		// not an error) used to be discarded here, so this always reported
		// success regardless of whether anything was actually deleted.
		return fs::remove(name);
	}
	catch (const fs::filesystem_error& ex)
	{
		if (ll != LogLevel::None) // TODO use handleLastError
            Logging::instance().log(ll, L"Error removing file: " + string_to_wstring(ex.what()));
		return false;
	}
    catch (...) {
        LOG_ERROR3(L"remove_file: ", name, L"Unbekannter Fehler")
        return false;
    }
}

bool set_last_write_time(const std::wstring& fn, const fs::file_time_type& modify_time, LogLevel ll)
{
	try {
		fs::last_write_time(fn, modify_time);
	}
	catch (const fs::filesystem_error& ex) {
        if (ll != LogLevel::None) // TODO use handleLastError
            Logging::instance().log(ll, L"Error setting last write time: " + string_to_wstring(ex.what()));
		return false;
	}
    catch (...) {
        LOG_ERROR3(L"set_last_write_time: ", fn, L"Unbekannter Fehler")
    }
	return true;
}

} // namespace util

} // namespace AFS_Core
