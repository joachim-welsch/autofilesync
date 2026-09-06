#include "pch.h"
#include "Directory.h"
#include <fstream>
#include <filesystem>

namespace AFS_Core {

namespace fs = std::filesystem;

using std::wstring;

WStringVector Directory::read(const wstring& dir)
{
    const wstring actDir = current_ + dir + L"\\";
    WStringVector retval;
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

} // namespace AFS_Core
