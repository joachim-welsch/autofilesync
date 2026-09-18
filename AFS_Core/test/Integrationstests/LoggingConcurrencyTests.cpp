#include "pch.h"
#include "Logging.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <FileUtil.h>
#include <ThStrUtil.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <StrUtil.h>
#include <ScopedEnv.h>
#endif

using namespace AFS_Core;

using tu::path_contains;
using tu::to_utf8;
namespace fs = std::filesystem;

TEST(LoggingConcurrencyTest, ConcurrentRotationAndLoggingDoesNotCrash)  // [LoggingConcurrencyTests.md#ConcurrentRotationAndLoggingDoesNotCrash]
{
    const fs::path log_file = fs::temp_directory_path() / ("autofilesync_logging_test_" + std::to_string(getpid()) + ".log");
    fs::remove(log_file);

    Logging::instance().set_log_file_path_name(log_file.wstring());
    Logging::instance().set_log_level(LogLevel::Debug);

    std::atomic<bool> stop{false};

    std::thread rotator([&] {
        while (!stop.load()) {
            Logging::instance().close_file();
            Logging::instance().open_file();
        }
    });

    std::vector<std::thread> loggers;
    for (int i = 0; i < 4; ++i) {
        loggers.emplace_back([&] {
            while (!stop.load()) {
                try {
                    log_info(L"concurrency test message");
                } catch (const std::exception&) {  // transient I/O errors are not what this test is about
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    rotator.join();
    for (auto& t : loggers)
        t.join();

    Logging::instance().close_file();
    fs::remove(log_file);
}

TEST(LoggingTests, WriteFailureDoesNotThrow)  // [LoggingConcurrencyTests.md#WriteFailureDoesNotThrow]
{
    const std::wstring previous_path = Logging::instance().get_log_file_path_name();
    const LogLevel previous_level = Logging::instance().get_log_level();
    Logging::instance().set_log_file_path_name(L"/dev/full");
    Logging::instance().set_log_level(LogLevel::Error);
    EXPECT_NO_THROW(Logging::instance().log(LogLevel::Error, std::string("write failure probe")));
    Logging::instance().set_log_level(previous_level);
    if (!previous_path.empty())
        Logging::instance().set_log_file_path_name(previous_path);
}

TEST(LoggingTests, OpenFileOnUnwritablePathDoesNotThrow)  // [LoggingConcurrencyTests.md#OpenFileOnUnwritablePathDoesNotThrow]
{
    const std::wstring previous_path = Logging::instance().get_log_file_path_name();
    const std::filesystem::path blocker = std::filesystem::temp_directory_path()  // [LoggingConcurrencyTests.md#blocked-parent-path]
        / ("afx_logging_blocker_" + std::to_string(getpid()));
    { std::ofstream(blocker) << "not a directory"; }
    const std::wstring bad_path = (blocker / "sub" / "afx.log").wstring();

    EXPECT_NO_THROW(Logging::instance().set_log_file_path_name(bad_path));

    if (!previous_path.empty())
        Logging::instance().set_log_file_path_name(previous_path);
    std::filesystem::remove(blocker);
}

#ifndef _WIN32
TEST(LoggingTests, DefaultLogFilePathHonorsHomeEnvVar)  // [LoggingConcurrencyTests.md#DefaultLogFilePathHonorsHomeEnvVar]
{
    const char* saved_home = getenv("HOME");
    const char* saved_xdg_cache = getenv("XDG_CACHE_HOME");
    unsetenv("XDG_CACHE_HOME");
    setenv("HOME", "/tmp/afx_fake_home_for_logging_test", 1);

    const std::wstring path = get_default_log_file_path();
    EXPECT_NE(path.find(L"/tmp/afx_fake_home_for_logging_test"), std::wstring::npos);
    EXPECT_NE(path.find(L"autofilesync"), std::wstring::npos);

    if (saved_home) setenv("HOME", saved_home, 1); else unsetenv("HOME");
    if (saved_xdg_cache) setenv("XDG_CACHE_HOME", saved_xdg_cache, 1); else unsetenv("XDG_CACHE_HOME");
}
#else
TEST(LoggingTests, DefaultLogFilePathHonorsLocalAppDataEnvVar)
{
    ScopedEnv local("LOCALAPPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Local)");  // [LoggingConcurrencyTests.md#fake-home-for-every-variable]
    ScopedEnv roaming("APPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Roaming)");
    ScopedEnv profile("USERPROFILE", R"(C:\Temp\afx_fake_home_for_logging_test)");

    const std::wstring path = get_default_log_file_path();

    EXPECT_TRUE(path_contains(path, L"C:/Temp/afx_fake_home_for_logging_test")) << to_utf8(path);
    EXPECT_TRUE(path_contains(path, L"autofilesync")) << to_utf8(path);
}
#endif

namespace {

class ScratchLogTest : public ::testing::Test {  // [LoggingConcurrencyTests.md#ScratchLogTest]
protected:
    const std::filesystem::path log_path_ = std::filesystem::temp_directory_path() / ("autofilesync_logorigin_" + std::to_string(getpid()) + ".log");
    std::wstring previous_log_path_;
    LogLevel previous_level_ = LogLevel::Info;

    void SetUp() override
    {
        previous_log_path_ = Logging::instance().get_log_file_path_name();
        previous_level_ = Logging::instance().get_log_level();
        std::filesystem::remove(log_path_);
        Logging::instance().set_log_file_path_name(log_path_.wstring());
        Logging::instance().set_log_level(LogLevel::Info);
    }

    void TearDown() override
    {
        Logging::instance().origin(LogOrigin::Gui);
        Logging::instance().set_log_level(previous_level_);
        Logging::instance().set_log_file_path_name(previous_log_path_);
        std::filesystem::remove(log_path_);
    }

    std::string line_with(const std::string& text) const
    {
        std::ifstream log(log_path_);
        for (std::string line; std::getline(log, line);)
            if (line.find(text) != std::string::npos)
                return line;
        return {};
    }
};

constexpr std::size_t TIMESTAMP_LENGTH = 23;  // "2026-09-18 09:26:23.138"

} // namespace

TEST_F(ScratchLogTest, GuiIsTheDefaultOrigin)
{
    log_info(L"message of the gui");
    EXPECT_EQ(line_with("message of the gui").substr(TIMESTAMP_LENGTH), " G Info\tmessage of the gui");
}

TEST_F(ScratchLogTest, DaemonMarksItsLinesWithD)
{
    Logging::instance().origin(LogOrigin::Daemon);
    log_error(L"message of the daemon");
    EXPECT_EQ(line_with("message of the daemon").substr(TIMESTAMP_LENGTH), " D Error\tmessage of the daemon");
}

#ifndef _WIN32
TEST_F(ScratchLogTest, WritingFollowsAFileReplacedByAnotherProcess)  // [LoggingConcurrencyTests.md#WritingFollowsAFileReplacedByAnotherProcess]
{
    log_info(L"before the replacement");
    const std::filesystem::path shortened = log_path_.string() + ".tmp";
    std::ofstream(shortened) << "shortened by the other process\n";
    std::filesystem::rename(shortened, log_path_);

    log_info(L"after the replacement");

    EXPECT_FALSE(line_with("shortened by the other process").empty());
    EXPECT_FALSE(line_with("after the replacement").empty()) << "the message went to the replaced, nameless file";
}

TEST_F(ScratchLogTest, WritingRecreatesADeletedFile)
{
    log_info(L"before the deletion");
    std::filesystem::remove(log_path_);

    log_info(L"after the deletion");

    EXPECT_FALSE(line_with("after the deletion").empty());
}
#endif
