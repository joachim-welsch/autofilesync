#pragma once
#include <string>
#include <filesystem>
#include <boost/json.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace util {

std::string toupper(std::string);
std::string withTrailingChar(const std::string&, char);

inline
constexpr char const* toStr(bool b)
{
   return b ? "true" : "false";
}

inline
std::string toString(bool b)
{
   return toStr(b);
}

    std::wstring withTrailingChar(const std::wstring& s, wchar_t c);
    bool fileExists(const std::string& StrFilename);
    bool dirExists(const char* name);
    void createDir(const char* name);
    std::string withoutExtension(const std::string& pathname);

    void handleLastError(const char *msg);
    std::string homeDir();
    bool fileinfo(const std::string& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
    std::string filename(const std::string& pathstr);
    //void copyFile(const std::string& source, const std::string& dest);
    boost::property_tree::ptree parseJsonFile( char const* filename);
}
