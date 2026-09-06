#pragma once
#include <string>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <Logging.h>

namespace AFS_Core {

namespace util {

    std::string wstring_to_string(const std::wstring& ws);
    std::wstring string_to_wstring(const std::string& s);

    unsigned long currentPid();

    inline uint16_t to_uint16_t(const std::wstring& s)
    {
		return static_cast<uint16_t>(std::stoi(s));
	}

    inline
    constexpr wchar_t const* toStr(bool b)
    {
        return b ? L"true" : L"false";
    }

    inline
    std::wstring toString(bool b)
    {
        return toStr(b);
    }
    inline
    std::wstring toString(uint16_t i)
    {
        return std::to_wstring(i);
    }

    inline
    std::wstring toString(int i)
    {
        return std::to_wstring(i);
    }

    std::string toString(const std::filesystem::file_time_type& ft);

    std::wstring withTrailingChar(const std::wstring& s, wchar_t c);
    
    bool volumeExists(const std::wstring& driveName, const std::wstring& volumeName);
    bool driveExists(const std::wstring& driveName);
    // Puts "YYYY-MM-DD_hh-mm-ss-mmm_" in front of the file name, leaving the
    // directory part untouched. The time defaults to the moment of the call;
    // pass one explicitly to get a reproducible name.
    std::filesystem::path withTimestampPrefix(const std::filesystem::path& pathname,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
    void handleLastError(std::filesystem::filesystem_error const& ex);
    bool wildmatch(const std::wstring& input, const std::wstring& wildcard);
    bool filesEqual(const std::wstring& input1, const std::wstring& input2);
    bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
    bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
	bool rename_file(const std::wstring& oldname, const std::wstring& newname);
    bool remove_file(const std::wstring& name, LogLevel ll = LogLevel::None);
	bool set_last_write_time(const std::wstring& fn, const std::filesystem::file_time_type& modify_time, LogLevel ll = LogLevel::None);
} // namespace util

} // namespace AFS_Core
