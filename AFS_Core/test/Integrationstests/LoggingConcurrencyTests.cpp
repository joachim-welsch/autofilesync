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

using tu::pathContains;
using tu::toUtf8;
namespace fs = std::filesystem;

// Regression test for Fehler 4: Logging::write() read and used the shared
// FILE* fos_ without holding mutex_, while closeFile()/openFile() (triggered
// by log rotation, e.g. LogCut) did lock it. A rotation racing an in-flight
// write could fclose() fos_ out from under it, leaving write() to dereference
// a dangling or null FILE*. Hammering rotation and logging concurrently from
// multiple threads must not crash.
TEST(LoggingConcurrencyTest, ConcurrentRotationAndLoggingDoesNotCrash)
{
    const fs::path logFile = fs::temp_directory_path() / ("autofilesync_logging_test_" + std::to_string(getpid()) + ".log");
    fs::remove(logFile);

    Logging::instance().setLogFilePathName(logFile.wstring());
    Logging::instance().setLogLevel(LogLevel::Debug);

    std::atomic<bool> stop{false};

    std::thread rotator([&] {
        while (!stop.load()) {
            Logging::instance().closeFile();
            Logging::instance().openFile();
        }
    });

    std::vector<std::thread> loggers;
    for (int i = 0; i < 4; ++i) {
        loggers.emplace_back([&] {
            while (!stop.load()) {
                try {
                    LOG_INFO(L"concurrency test message");
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

    Logging::instance().closeFile();
    fs::remove(logFile);
}

// Fehler 4 (round 5): write() threw std::runtime_error when fprintf/fflush
// failed (e.g. disk full). LOG_ macros run inside catch handlers and worker
// threads, so that exception escaped them and terminated the process exactly
// when something was already going wrong. Logging to /dev/full makes the
// flush fail deterministically; it must be swallowed, not thrown.
TEST(LoggingTests, WriteFailureDoesNotThrow)
{
    const std::wstring previousPath = Logging::instance().getLogFilePathName();
    const LogLevel previousLevel = Logging::instance().getLogLevel();
    Logging::instance().setLogFilePathName(L"/dev/full");
    Logging::instance().setLogLevel(LogLevel::Error);
    EXPECT_NO_THROW(Logging::instance().log(LogLevel::Error, std::string("write failure probe")));
    Logging::instance().setLogLevel(previousLevel);
    if (!previousPath.empty())
        Logging::instance().setLogFilePathName(previousPath);
}

// Fehler (round 6): openFile() threw std::runtime_error when the log path
// could not be opened (missing/unwritable directory). setLogFilePathName()
// calls it right after closeFile()-ing the previous handle - reachable from
// LoggingDialog's OK handler with bad user input - and LogCut::closeFiles()
// calls it unconditionally at the end of every LogCut() run, which
// MainWindow's constructor invokes unguarded on every single application
// startup. Either path throwing would crash the whole app instead of just
// leaving logging disabled.
TEST(LoggingTests, OpenFileOnUnwritablePathDoesNotThrow)
{
    const std::wstring previousPath = Logging::instance().getLogFilePathName();
    // A path whose parent is a regular file, not a directory: create_directories
    // and fopen() can never succeed for it.
    const std::filesystem::path blocker = std::filesystem::temp_directory_path()
        / ("afx_logging_blocker_" + std::to_string(getpid()));
    { std::ofstream(blocker) << "not a directory"; }
    const std::wstring badPath = (blocker / "sub" / "afx.log").wstring();

    EXPECT_NO_THROW(Logging::instance().setLogFilePathName(badPath));

    if (!previousPath.empty())
        Logging::instance().setLogFilePathName(previousPath);
    std::filesystem::remove(blocker);
}

// Fehler 6 (round 6): the non-WIN32 default log path was a hardcoded
// developer path (/home/welschj/temp/autofilesync.log) - the Linux/macOS
// equivalent of getDefaultLogFilePath() (already implemented for Windows via
// GetTempPathW) simply didn't exist, so every install other than that one
// machine got a default log location nobody else could write to. Verified by
// pointing HOME/XDG_CACHE_HOME at a fake location and checking the computed
// path actually derives from them, rather than being a fixed string.
#ifndef _WIN32
TEST(LoggingTests, DefaultLogFilePathHonorsHomeEnvVar)
{
    const char* savedHome = getenv("HOME");
    const char* savedXdgCache = getenv("XDG_CACHE_HOME");
    unsetenv("XDG_CACHE_HOME");
    setenv("HOME", "/tmp/afx_fake_home_for_logging_test", 1);

    const std::wstring path = getDefaultLogFilePath();
    EXPECT_NE(path.find(L"/tmp/afx_fake_home_for_logging_test"), std::wstring::npos);
    EXPECT_NE(path.find(L"autofilesync"), std::wstring::npos);

    if (savedHome) setenv("HOME", savedHome, 1); else unsetenv("HOME");
    if (savedXdgCache) setenv("XDG_CACHE_HOME", savedXdgCache, 1); else unsetenv("XDG_CACHE_HOME");
}
#else
TEST(LoggingTests, DefaultLogFilePathHonorsLocalAppDataEnvVar)
{
    // Alle drei setzen, damit auch der Fallback-Pfad ins Fake-Verzeichnis zeigt
    // und der Test nicht versehentlich ins echte Profil schreibt.
    ScopedEnv local("LOCALAPPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Local)");
    ScopedEnv roaming("APPDATA", R"(C:\Temp\afx_fake_home_for_logging_test\Roaming)");
    ScopedEnv profile("USERPROFILE", R"(C:\Temp\afx_fake_home_for_logging_test)");

    const std::wstring path = getDefaultLogFilePath();

    EXPECT_TRUE(pathContains(path, L"C:/Temp/afx_fake_home_for_logging_test")) << toUtf8(path);
    EXPECT_TRUE(pathContains(path, L"autofilesync")) << toUtf8(path);
}
#endif
