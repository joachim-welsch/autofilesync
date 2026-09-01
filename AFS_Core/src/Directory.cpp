#include "pch.h"
#include "Directory.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using std::wstring;

svect Directory::read(const wstring& dir)
{
    const wstring actDir = current_ + dir + L"\\";
    svect retval;
    for (const fs::directory_entry& dir_entry :
        std::filesystem::directory_iterator(actDir))
    {
        const auto& newpath = dir_entry.path();
        if (newpath.has_filename()) {
            std::wstring fname = newpath.filename().generic_wstring();
            retval.push_back(fname);
        }
    }
    return retval;
}
