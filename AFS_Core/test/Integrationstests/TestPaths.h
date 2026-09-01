#pragma once
#include <string>
#include <filesystem>

// Test fixtures need two kinds of on-disk locations: read-only resources
// checked into the repo, and a writable scratch area for sync source/target
// dirs. Both used to be hardcoded to one developer's Linux home directory
// (/home/welschj/...), which only exists on that one machine. LIB_SOURCE_DIR
// (set from $HOME by CMake) and the OS temp dir make the fixtures work on
// any machine and platform.
namespace TestPaths
{
    inline std::wstring repoDir()
    {
        static const std::string dir = LIB_SOURCE_DIR;
        return std::wstring(dir.begin(), dir.end());
    }

    inline std::wstring scratchDir()
    {
        static const std::wstring dir = (std::filesystem::temp_directory_path() / "autofilesync_test").wstring();
        return dir;
    }
}
