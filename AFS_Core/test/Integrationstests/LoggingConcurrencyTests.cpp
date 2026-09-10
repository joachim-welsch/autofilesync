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

// Regression test for Fehler 4: Logging::write() read and used the shared
// FILE* fos_ without holding mutex_, while close_file()/open_file() (triggered
// by log rotation, e.g. LogCut) did lock it. A rotation racing an in-flight
// write could fclose() fos_ out from under it, leaving write() to dereference
// a dangling or null FILE*. Hammering rotation and logging concurrently from
// multiple threads must not crash.
TEST(LoggingConcurrencyTest, ConcurrentRotationAndLoggingDoesNotCrash)
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
                } catch (const std::exception&) {
                    // transient I/O errors are not what this test is about
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

// Fehler 4 (round 5): write() threw std::runtime_error when fprintf/fflush
// failed (e.g. disk full). LOG_ macros run inside catch handlers and worker
// threads, so that exception escaped them and terminated the process exactly
// when something was already going wrong. Logging to /dev/full makes the
// flush fail deterministically; it must be swallowed, not thrown.
TEST(LoggingTests, WriteFailureDoesNotThrow)
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

// Fehler (round 6): open_file() threw std::runtime_error when the log path
// could not be opened (missing/unwritable directory). set_log_file_path_name()
// calls it right after close_file()-ing the previous handle - reachable from
// LoggingDialog's OK handler with bad user input - and LogCut::close_files()
// calls it unconditionally at the end of every LogCut() run, which
// MainWindow's constructor invokes unguarded on every single application
// startup. Either path throwing would crash the whole app instead of just
// leaving logging disabled.
TEST(LoggingTests, OpenFileOnUnwritablePathDoesNotThrow)
{
    const std::wstring previous_path = Logging::instance().get_log_file_path_name();
    // A path whose parent is a regular file, not a directory: create_directories
    // and fopen() can never succeed for it.
    const std::filesystem::path blocker = std::filesystem::temp_directory_path()
        / ("afx_logging_blocker_" + std::to_string(getpid()));
    { std::ofstream(blocker) << "not a directory"; }
    const std::wstring bad_path = (blocker / "sub" / "afx.log").wstring();

    EXPECT_NO_THROW(Logging::instance().set_log_file_path_name(bad_path));

    if (!previous_path.empty())
        Logging::instance().set_log_file_path_name(previous_path);
    std::filesystem::remove(blocker);
}

// Fehler 6 (round 6): the non-WIN32 default log path was a hardcoded
// developer path (/home/welschj/temp/autofilesync.log) - the Linux/macOS
// equivalent of get_default_log_file_path() (already implemented for Windows via
// GetTempPathW) simply didn't exist, so every install other than that one
// machine got a default log location nobody else could write to. Verified by
// pointing HOME/XDG_CACHE_HOME at a fake location and checking the computed
// path actually derives from them, rather than being a fixed string.
#ifndef _WIN32
TEST(LoggingTests, DefaultLogFilePathHonorsHomeEnvVar)
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
    // Alle drei setzen, damit auch der Fallback-Pfad ins Fake-Verzeichnis zeigt
    // und der Test nicht versehentlich ins echte Profil schreibt.
    ScopedEnv local("LOCALAPPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Local)");
    ScopedEnv roaming("APPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Roaming)");
    ScopedEnv profile("USERPROFILE", R"(C:\Temp\afx_fake_home_for_logging_test)");

    const std::wstring path = get_default_log_file_path();

    EXPECT_TRUE(path_contains(path, L"C:/Temp/afx_fake_home_for_logging_test")) << to_utf8(path);
    EXPECT_TRUE(path_contains(path, L"autofilesync")) << to_utf8(path);
}
#endif
