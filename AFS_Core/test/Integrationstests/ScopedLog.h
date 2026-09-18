#pragma once
#include <Logging.h>
#include <filesystem>

namespace tu {

class ScopedLog {  // [ScopedLog.md#ScopedLog]
    std::filesystem::path path_;
    std::wstring previous_path_ = AFS_Core::Logging::instance().get_log_file_path_name();
    AFS_Core::LogLevel previous_level_ = AFS_Core::Logging::instance().get_log_level();
public:
    explicit ScopedLog(std::filesystem::path path, AFS_Core::LogLevel level = AFS_Core::LogLevel::Info): path_(std::move(path))
    {
        std::filesystem::remove(path_);
        AFS_Core::Logging::instance().set_log_file_path_name(path_.wstring());
        AFS_Core::Logging::instance().set_log_level(level);
    }
    ScopedLog(const ScopedLog&) = delete;
    ScopedLog& operator=(const ScopedLog&) = delete;
    ~ScopedLog()
    {
        AFS_Core::Logging::instance().set_log_level(previous_level_);
        AFS_Core::Logging::instance().set_log_file_path_name(previous_path_);
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }
};

} // namespace tu
