#pragma once
#include <RecycleBin.h>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace tu {

class ScopedRecycleBin {  // [ScopedRecycleBin.md#ScopedRecycleBin]
    std::filesystem::path previous_ = AFS_Core::RecycleBin::instance().bin_path();
    std::filesystem::path path_ = std::filesystem::temp_directory_path() / ("autofilesync_recyclebin_" + std::to_string(getpid()));
public:
    ScopedRecycleBin()
    {
        std::filesystem::remove_all(path_);
        AFS_Core::RecycleBin::instance().bin_path(path_);
    }
    ScopedRecycleBin(const ScopedRecycleBin&) = delete;
    ScopedRecycleBin& operator=(const ScopedRecycleBin&) = delete;
    ~ScopedRecycleBin()
    {
        AFS_Core::RecycleBin::instance().bin_path(previous_);
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }
};

} // namespace tu
