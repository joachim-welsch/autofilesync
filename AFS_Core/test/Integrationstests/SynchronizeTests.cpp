#include "pch.h"
#include "Synchronize.h"
#include "Config.h"
#include "SshTest.h"
#include "FileUtil.h"
#include "DirUtil.h"
#include <util.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>

// Regression test for Fehler 7: the worker thread used to start before stop_
// was reset to false, so a thread that reached its "while (!stop_)" check
// before create() got to run could see the stale "true" left over from
// program start and silently exit after a single pass instead of looping.

using namespace AFS_Core;

TEST(SynchronizeTests, ContinuousSyncKeepsRunningPastFirstPass)
{
    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    // cv_.wait_for() inside the loop times out after 1s; waiting past that
    // guarantees a second pass has started if the loop is still alive.
    std::this_thread::sleep_for(std::chrono::milliseconds(1300));

    EXPECT_GE(sync->loopIterations(), 2);

    Synchronize::destroy(); // stops and joins the worker thread
}

// Regression test for Fehler 8: CheckPath uses the throwing fs:: overloads, so
// a single item whose path is inaccessible (e.g. permission denied) used to
// throw out of Synchronize::syncItem() uncaught, aborting the whole sync pass
// - including every other configured item - instead of just skipping that one.
TEST(SynchronizeTests, OneItemFilesystemErrorDoesNotStopOtherItems)
{
    namespace fs = std::filesystem;
    const fs::path testDir = fs::temp_directory_path() / ("autofilesync_syncitem_test_" + std::to_string(getpid()));
    fs::remove_all(testDir);
    fs::create_directories(testDir / "blocked" / "child");
    fs::create_directories(testDir / "goodSource");
    fs::create_directories(testDir / "goodTarget");
    fs::permissions(testDir / "blocked", fs::perms::none);

    Config::instance().reset();
    Config::instance().add(SyncItem(L"bad", SyncItemType::Dir,
        (testDir / "blocked" / "child").wstring(), (testDir / "badTarget").wstring(), L""));
    Config::instance().add(SyncItem(L"good", SyncItemType::Dir,
        (testDir / "goodSource").wstring(), (testDir / "goodTarget").wstring(), L""));

    Synchronize* sync = Synchronize::create(SyncType::Single);
    ASSERT_TRUE(sync != nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // The second item's lastSync() only updates if syncItem() actually reached
    // it, i.e. the exception from the first item did not abort the loop.
    EXPECT_NE(Config::instance().syncitems()[1].lastSync(), (std::chrono::steady_clock::time_point::min)());

    Synchronize::destroy();
    fs::permissions(testDir / "blocked", fs::perms::owner_all);
    fs::remove_all(testDir);
    Config::instance().reset();
}

TEST(SynchronizeTests, CheckDestruction)
{
    Synchronize* sync=Synchronize::create(SyncType::Single);
    EXPECT_TRUE(sync != nullptr);
    Synchronize::destroy();
    EXPECT_FALSE(Synchronize::hasInstance());
}

TEST(SynchronizeTests, CheckPathExists)
{
#ifdef _UNIX
    EXPECT_FALSE(Synchronize::create(SyncType::Single)->pathExists(L"/home/welschj/temp/notexisting"));
#else
    EXPECT_FALSE(Synchronize::create(SyncType::Single)->pathExists(L"C:\\temp\\notexisting"));
#endif
    Synchronize::destroy();
}

#include "SyncFileSsh.h"

// Fehler 10 (round 4): every sync pass called sshIsDirectory() for a File item
// with an SSH target - a complete SSH connect/handshake/auth/teardown per pass,
// hammering the server once a second in continuous sync. The result is now
// cached per target with a TTL in Synchronize::isSshTargetDir(), so repeated
// passes must not reconnect for this check.
TEST(SynchronizeTests, SshTargetDirCheckIsCachedAcrossPasses)
{
    // Frueher genuegte hier ein leerer Host, weil CheckPath jeden SSH-Pfad
    // ungeprueft durchliess und der Durchlauf trotzdem bis zur
    // Zielverzeichnis-Pruefung kam. Seit CheckPath die Gegenstelle wirklich
    // prueft, setzt ein unerreichbares Ziel den Eintrag vorher aus - der Test
    // braucht deshalb einen erreichbaren Server.
    SKIP_WITHOUT_SSH_SERVER();
    Config::instance().reset();
    SshAccount acc;
    acc.setName("localhost");
    acc.setUser(SSHTest::SSH_USER);
    acc.setHost(SSHTest::SSH_HOST);
    acc.setPassword(SSHTest::SSH_PASSWORD);
    Config::instance().add(acc, true);

    // Vorhandene Quelldatei und vorhandenes Zielverzeichnis: erst dann kommt
    // der Durchlauf ueberhaupt bis zu der Pruefung, um die es hier geht.
    const std::string dir = tu::concatPath(tu::testTempDir(), "afx_sshcache");
    std::filesystem::create_directories(dir);
    const std::string srcFile = tu::concatPath(dir, "quelle.txt");
    std::ofstream(srcFile) << "Inhalt";

    // Account must exist before the SyncItem is built, or the target does not
    // parse as an SshPath.
    SyncItem si(L"sshitem", SyncItemType::File, util::string_to_wstring(srcFile),
                L"localhost:" + util::string_to_wstring(dir), L"*");
    si.syncCycle(0);
    Config::instance().add(si, true);

    sshIsDirectoryCallCount = 0;
    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loopIterations() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(sync->loopIterations(), 3);
    Synchronize::destroy();

    // Every pass runs the check, but only the first may actually connect.
    EXPECT_EQ(sshIsDirectoryCallCount.load(), 1);
    Config::instance().reset();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// Fehler 6 (round 7): ProgressDialog::OnCancel() called the blocking
// SyncControler::stopSync() directly on the GUI thread - its
// std::this_thread::sleep_for()-based wait loop doesn't pump the event queue,
// freezing the whole window for however long the worker took to notice the
// stop request. The new requestStop() only signals and must return
// immediately, leaving the actual (now GUI-responsive) waiting to the caller.
#include "SyncControler.h"
TEST(SyncControlerTests, RequestStopReturnsImmediatelyUnlikeStopSync)
{
    Config::instance().reset();
    auto& sc = SyncControler::instance();
    sc.resetState();
    ASSERT_TRUE(sc.startSync(SyncType::Continuous));

    const auto start = std::chrono::steady_clock::now();
    sc.requestStop();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsedMs, 200) << "requestStop() must signal and return, not wait for the worker to finish";

    // Cleanup via destroySynchronize() (its ~Synchronize() signals stop_ and
    // joins the worker thread directly), not sc.syncFinished(): state_ only
    // transitions to SyncFinished under #ifdef _WXWIDGETS - not compiled into
    // this wx-less test binary - so waiting on it here would hang forever.
    sc.destroySynchronize();
    sc.resetState();
    Config::instance().reset();
}
