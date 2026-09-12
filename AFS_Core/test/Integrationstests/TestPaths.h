#pragma once
#include <string>
#include <filesystem>

namespace TestPaths  // [TestPaths.md#TestPaths]
{
    inline std::wstring repo_dir()
    {
        static const std::string dir = LIB_SOURCE_DIR;
        return std::wstring(dir.begin(), dir.end());
    }

    inline std::wstring scratch_dir()
    {
        static const std::wstring dir = (std::filesystem::temp_directory_path() / "autofilesync_test").wstring();
        return dir;
    }
}
