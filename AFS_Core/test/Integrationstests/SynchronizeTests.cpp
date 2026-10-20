/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "Synchronize.h"
#include "Config.h"
#include "SshTest.h"
#include "FileUtil.h"
#include "DirUtil.h"
#include "TestPaths.h"
#include <Util.h>
#include <Logging.h>
#include <Translation.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <optional>

using namespace AFS_Core;  // [SynchronizeTests.md#stop-flag-regression]

TEST(SynchronizeTests, ContinuousSyncKeepsRunningPastFirstPass)
{
    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1300));  // [SynchronizeTests.md#second-pass-timing]

    EXPECT_GE(sync->loop_iterations(), 2);

    Synchronize::destroy(); // stops and joins the worker thread
}

TEST(SynchronizeTests, OneItemFilesystemErrorDoesNotStopOtherItems)  // [SynchronizeTests.md#OneItemFilesystemErrorDoesNotStopOtherItems]
{
    namespace fs = std::filesystem;
    const fs::path test_dir = fs::temp_directory_path() / ("autofilesync_syncitem_test_" + std::to_string(getpid()));
    fs::remove_all(test_dir);
    fs::create_directories(test_dir / "blocked" / "child");
    fs::create_directories(test_dir / "goodSource");
    fs::create_directories(test_dir / "goodTarget");
    fs::permissions(test_dir / "blocked", fs::perms::none);

    Config::instance().reset();
    Config::instance().add(SyncItem(L"bad", SyncItemType::Dir,
        (test_dir / "blocked" / "child").wstring(), (test_dir / "badTarget").wstring(), L""));
    Config::instance().add(SyncItem(L"good", SyncItemType::Dir,
        (test_dir / "goodSource").wstring(), (test_dir / "goodTarget").wstring(), L""));

    Synchronize* sync = Synchronize::create(SyncType::Single);
    ASSERT_TRUE(sync != nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_NE(Config::instance().syncitems()[1].last_sync(), (std::chrono::steady_clock::time_point::min)());  // [SynchronizeTests.md#second-item-reached]

    Synchronize::destroy();
    fs::permissions(test_dir / "blocked", fs::perms::owner_all);
    fs::remove_all(test_dir);
    Config::instance().reset();
}

TEST(SynchronizeTests, CheckDestruction)
{
    Synchronize* sync=Synchronize::create(SyncType::Single);
    EXPECT_TRUE(sync != nullptr);
    Synchronize::destroy();
    EXPECT_FALSE(Synchronize::has_instance());
}

TEST(SynchronizeTests, CheckPathExists)
{
    EXPECT_FALSE(Synchronize::create(SyncType::Single)->path_exists(TestPaths::scratch_dir() + L"/notexisting"));
    Synchronize::destroy();
}

#include "SyncFileSsh.h"

TEST(SynchronizeTests, SshTargetDirCheckIsCachedAcrossPasses)  // [SynchronizeTests.md#SshTargetDirCheckIsCachedAcrossPasses]
{
    SKIP_WITHOUT_SSH_SERVER();  // [SynchronizeTests.md#needs-reachable-server]
    Config::instance().reset();
    SshAccount acc;
    acc.set_name("localhost");
    acc.set_user(SSHTest::SSH_USER);
    acc.set_host(SSHTest::SSH_HOST);
    acc.set_password(SSHTest::SSH_PASSWORD);
    Config::instance().add(acc, true);

    const std::string dir = tu::concat_path(tu::test_temp_dir(), "afx_sshcache");  // [SynchronizeTests.md#preconditions-for-check]
    std::filesystem::create_directories(dir);
    const std::string src_file = tu::concat_path(dir, "quelle.txt");
    std::ofstream(src_file) << "Inhalt";

    SyncItem si(L"sshitem", SyncItemType::File, util::string_to_wstring(src_file),  // [SynchronizeTests.md#account-before-syncitem]
                L"localhost:" + util::string_to_wstring(dir), L"*");
    si.sync_cycle(0);
    Config::instance().add(si, true);

    ssh_is_directory_call_count = 0;
    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loop_iterations() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(sync->loop_iterations(), 3);
    Synchronize::destroy();

    EXPECT_EQ(ssh_is_directory_call_count.load(), 1);  // every pass runs the check, but only the first may actually connect
    Config::instance().reset();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

namespace {
int count_lines_containing(const std::filesystem::path& file, const std::string& text)
{
    std::ifstream in(file);
    int count = 0;
    for (std::string line; std::getline(in, line);)
        if (line.find(text) != std::string::npos)
            ++count;
    return count;
}
}

TEST(SynchronizeTests, NewerTargetIsLoggedExactlyOnceAcrossPasses)  // [SynchronizeTests.md#NewerTargetIsLoggedExactlyOnceAcrossPasses]
{
    namespace fs = std::filesystem;
    using namespace std::chrono;
    const fs::path test_dir = fs::temp_directory_path() / ("autofilesync_newertarget_passes_" + std::to_string(getpid()));
    fs::remove_all(test_dir);
    fs::create_directories(test_dir / "source");
    fs::create_directories(test_dir / "target");
    const fs::path source = test_dir / "source" / "vokabeln.txt";
    const fs::path target = test_dir / "target" / "vokabeln.txt";
    std::ofstream(source) << "alter Stand";
    std::ofstream(target) << "neuer Stand";
    fs::last_write_time(source, fs::last_write_time(target) - hours(1));
    const std::wstring previous_log = Logging::instance().get_log_file_path_name();
    const LogLevel previous_level = Logging::instance().get_log_level();
    const fs::path log = test_dir / "sync.log";
    Logging::instance().set_log_file_path_name(log.wstring());
    Logging::instance().set_log_level(LogLevel::Info);

    Config::instance().reset();
    SyncItem si(L"newer target", SyncItemType::File, source.wstring(), target.wstring(), L"*");
    si.sync_cycle(0);
    CompareItems by_time;
    by_time.content(false);
    by_time.size(false);
    by_time.time(true);
    si.compare_items(by_time);
    Config::instance().add(si, true);

    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loop_iterations() < 4; ++i)
        std::this_thread::sleep_for(milliseconds(100));
    const int passes = sync->loop_iterations();
    Synchronize::destroy();

    EXPECT_GE(passes, 3) << "the message must have had several passes to repeat itself";
    EXPECT_EQ(count_lines_containing(log, util::wstring_to_string(translation::translate("target_newer_not_copied"))), 1);
    EXPECT_EQ(count_lines_containing(log, target.string()), 1) << "no other message may name the untouched target";
    std::ifstream kept(target);
    std::string content;
    std::getline(kept, content);
    EXPECT_EQ(content, "neuer Stand");

    Logging::instance().set_log_level(previous_level);
    Logging::instance().set_log_file_path_name(previous_log);
    Config::instance().reset();
    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

TEST(SynchronizeTests, ContentConflictIsLoggedExactlyOnceAcrossPasses)  // [SynchronizeTests.md#ContentConflictIsLoggedExactlyOnceAcrossPasses]
{
    namespace fs = std::filesystem;
    using namespace std::chrono;
    const fs::path test_dir = fs::temp_directory_path() / ("autofilesync_contentconflict_passes_" + std::to_string(getpid()));
    fs::remove_all(test_dir);
    fs::create_directories(test_dir / "source");
    fs::create_directories(test_dir / "target");
    const fs::path source = test_dir / "source" / "vokabeln.txt";
    const fs::path target = test_dir / "target" / "vokabeln.txt";
    std::ofstream(source) << "Stand A";
    std::ofstream(target) << "Stand B";
    fs::last_write_time(source, fs::last_write_time(target));
    const std::wstring previous_log = Logging::instance().get_log_file_path_name();
    const LogLevel previous_level = Logging::instance().get_log_level();
    const fs::path log = test_dir / "sync.log";
    Logging::instance().set_log_file_path_name(log.wstring());
    Logging::instance().set_log_level(LogLevel::Info);

    Config::instance().reset();
    SyncItem si(L"content conflict", SyncItemType::File, source.wstring(), target.wstring(), L"*");
    si.sync_cycle(0);
    si.two_way_sync(true);
    si.conflict_copies(false);  // the test is about the report, not the resolution (TwoWaySyncTests.md#conflict-copies)
    CompareItems all;
    all.content(true);
    all.size(true);
    all.time(true);
    si.compare_items(all);
    Config::instance().add(si, true);

    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loop_iterations() < 4; ++i)
        std::this_thread::sleep_for(milliseconds(100));
    const int passes = sync->loop_iterations();
    Synchronize::destroy();

    EXPECT_GE(passes, 3) << "the message must have had several passes to repeat itself";
    const std::string message = util::wstring_to_string(translation::translate("content_conflict_not_copied"));
    EXPECT_EQ(count_lines_containing(log, " Error\t" + message), 1);
    EXPECT_EQ(count_lines_containing(log, target.string()), 1) << "no other message may name the untouched target";
    std::string source_content, target_content;
    std::getline(std::ifstream(source) >> std::ws, source_content);
    std::getline(std::ifstream(target) >> std::ws, target_content);
    EXPECT_EQ(source_content, "Stand A");
    EXPECT_EQ(target_content, "Stand B");

    Logging::instance().set_log_level(previous_level);
    Logging::instance().set_log_file_path_name(previous_log);
    Config::instance().reset();
    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

#include "SyncControler.h"  // [SynchronizeTests.md#request-stop-must-not-block]
namespace {
std::optional<LastRun> last_run_after_one_pass(const std::wstring& source, const std::wstring& target)
{
    Config::instance().reset();
    Config::instance().add(SyncItem(L"lauf", SyncItemType::Dir, source, target, L"*"));
    Synchronize::create(SyncType::Single);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    Synchronize::destroy();
    const std::optional<LastRun> run = Config::instance().syncitems().at(0).last_run();
    Config::instance().reset();
    return run;
}
}

TEST(SynchronizeTests, LastRunIsRecordedInTheConfiguration)  // [SynchronizeTests.md#LastRunIsRecordedInTheConfiguration]
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("afs_lastrun_" + std::to_string(util::current_pid()));
    std::filesystem::create_directories(root / "quelle");
    std::filesystem::create_directories(root / "ziel");
    std::ofstream(root / "quelle" / "a.txt") << "a";

    const std::optional<LastRun> run = last_run_after_one_pass((root / "quelle").wstring(), (root / "ziel").wstring());

    std::filesystem::remove_all(root);
    ASSERT_TRUE(run.has_value()) << "Der Durchlauf hat nichts eingetragen.";
    EXPECT_TRUE(run->reachable);
    EXPECT_EQ(run->errors, 0u);
}

TEST(SynchronizeTests, UnreachableSourceIsRecordedAsSuch)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("afs_lastrun_" + std::to_string(util::current_pid()));
    std::filesystem::create_directories(root / "ziel");

    const std::optional<LastRun> run = last_run_after_one_pass((root / "gibt-es-nicht").wstring(), (root / "ziel").wstring());

    std::filesystem::remove_all(root);
    ASSERT_TRUE(run.has_value()) << "Der Durchlauf hat nichts eingetragen.";
    EXPECT_FALSE(run->reachable);
}

TEST(SyncControlerTests, RequestStopReturnsImmediatelyUnlikeStopSync)
{
    Config::instance().reset();
    auto& sc = SyncControler::instance();
    sc.reset_state();
    ASSERT_TRUE(sc.start_sync(SyncType::Continuous));

    const auto start = std::chrono::steady_clock::now();
    sc.request_stop();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsed_ms, 200) << "request_stop() must signal and return, not wait for the worker to finish";

    sc.destroy_synchronize();  // [SynchronizeTests.md#cleanup-via-destroy]
    sc.reset_state();
    Config::instance().reset();
}

namespace {

class SyncScheduleTest : public ::testing::Test {  // [SynchronizeTests.md#SyncScheduleTest]
protected:
    const std::filesystem::path root_ = std::filesystem::temp_directory_path() / ("afs_schedule_" + std::to_string(util::current_pid()));
    const std::filesystem::path source_ = root_ / "quelle";
    const std::filesystem::path target_ = root_ / "ziel";

    void SetUp() override
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(target_);
        tu::write_text_file(source_ / "a.txt", "a");
        Config::instance().reset();
    }

    void TearDown() override
    {
        if (Synchronize::has_instance())
            Synchronize::destroy();
        Config::instance().reset();
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    void add_item(uint16_t cycle_seconds, bool enabled = true) const
    {
        SyncItem si(L"lauf", SyncItemType::Dir, source_.wstring(), target_.wstring(), L"*");
        si.sync_cycle(cycle_seconds);
        si.enabled(enabled);
        Config::instance().add(si);
    }

    static Synchronize* pass(SyncType type, std::chrono::milliseconds wait)  // [SynchronizeTests.md#pass]
    {
        Synchronize* sync = Synchronize::create(type);
        std::this_thread::sleep_for(wait);
        return sync;
    }

    bool in_target(const char* name) const { return std::filesystem::exists(target_ / name); }
};

} // namespace

TEST_F(SyncScheduleTest, DisabledItemIsSkippedByARun)  // [SynchronizeTests.md#DisabledItemIsSkippedByARun]
{
    add_item(0, false);

    pass(SyncType::Single, std::chrono::milliseconds(400));

    EXPECT_FALSE(in_target("a.txt")) << "ein deaktiviertes Item darf nichts kopieren";
    const SyncItem item = Config::instance().syncitems().at(0);
    EXPECT_EQ(item.last_sync(), (std::chrono::steady_clock::time_point::min)()) << "ein uebersprungenes Item bekommt keinen Zeitstempel";
    EXPECT_FALSE(item.last_run().has_value()) << "ein uebersprungenes Item bekommt keinen letzten Lauf";
}

TEST_F(SyncScheduleTest, DisabledItemIsSkippedByThePreviewToo)
{
    add_item(0, false);

    Synchronize* sync = pass(SyncType::Preview, std::chrono::milliseconds(400));

    EXPECT_TRUE(sync->plan().empty());
    EXPECT_FALSE(in_target("a.txt"));
}

TEST_F(SyncScheduleTest, ItemWaitsForItsCycleBeforeItRunsAgain)  // [SynchronizeTests.md#cycle-timing]
{
    add_item(30);
    Synchronize* sync = pass(SyncType::Continuous, std::chrono::milliseconds(400));
    ASSERT_TRUE(in_target("a.txt")) << "der erste Durchlauf laeuft sofort";
    tu::write_text_file(source_ / "b.txt", "b");

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    EXPECT_GE(sync->loop_iterations(), 2) << "der Takt muss weitergelaufen sein, sonst sagt der Test nichts";
    EXPECT_FALSE(in_target("b.txt")) << "vor Ablauf der Pause darf das Item nicht erneut laufen";
}

TEST_F(SyncScheduleTest, ItemRunsAgainOnceItsCycleHasElapsed)  // [SynchronizeTests.md#cycle-timing]
{
    add_item(1);
    pass(SyncType::Continuous, std::chrono::milliseconds(400));
    ASSERT_TRUE(in_target("a.txt"));
    tu::write_text_file(source_ / "b.txt", "b");

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    EXPECT_TRUE(in_target("b.txt")) << "nach Ablauf der Pause muss das Item erneut laufen";
}

TEST_F(SyncScheduleTest, ASingleRunHonoursThePauseOfTheLastRun)  // [SynchronizeTests.md#cycle-timing]
{
    add_item(30);
    pass(SyncType::Single, std::chrono::milliseconds(400));
    Synchronize::destroy();
    ASSERT_TRUE(in_target("a.txt"));
    tu::write_text_file(source_ / "b.txt", "b");

    pass(SyncType::Single, std::chrono::milliseconds(400));

    EXPECT_FALSE(in_target("b.txt")) << "auch ein einzelner Lauf wartet die Pause seit dem letzten Lauf ab";
}

TEST_F(SyncScheduleTest, PreviewIgnoresThePause)  // [SynchronizeTests.md#PreviewIgnoresThePause]
{
    add_item(30);
    pass(SyncType::Single, std::chrono::milliseconds(400));
    Synchronize::destroy();
    ASSERT_TRUE(in_target("a.txt"));
    tu::write_text_file(source_ / "b.txt", "b");

    Synchronize* sync = pass(SyncType::Preview, std::chrono::milliseconds(400));

    const SyncPlan& plan = sync->plan();
    ASSERT_EQ(plan.steps().size(), 1u) << "die Vorschau muss das Item trotz laufender Pause ansehen";
    EXPECT_EQ(std::filesystem::path(plan.steps().front().source).filename(), "b.txt");
    EXPECT_FALSE(in_target("b.txt")) << "die Vorschau kopiert nichts";
}

namespace {
    struct WatchedRun {  // [SynchronizeTests.md#watched-run]
        const std::filesystem::path dir = std::filesystem::temp_directory_path() / "autofilesync_watched_run";
        const std::filesystem::path source = dir / "source";
        const std::filesystem::path target = dir / "target";
        explicit WatchedRun(bool watch)
        {
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(source);
            std::filesystem::create_directories(target);
            std::ofstream(source / "erste.txt") << "1";
            Config::instance().reset();
            SyncItem si(L"watched", SyncItemType::Dir, source.wstring(), target.wstring(), L"");
            si.sync_cycle(60);
            si.watch_changes(watch);
            Config::instance().add(si, true);
        }
        ~WatchedRun()
        {
            Synchronize::destroy();
            Config::instance().reset();
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
        bool appears(const char* name, int timeout_ms) const
        {
            for (int waited = 0; waited < timeout_ms; waited += 50) {
                if (std::filesystem::exists(target / name))
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return false;
        }
    };
}

TEST(SynchronizeTests, AWatchedItemSyncsANewFileLongBeforeItsPause)  // [SynchronizeTests.md#watched-run]
{
    const WatchedRun run(true);
    Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(run.appears("erste.txt", 5000)) << "the first pass runs at once, watched or not";

    std::ofstream(run.source / "zweite.txt") << "2";

    EXPECT_TRUE(run.appears("zweite.txt", 8000)) << "the change must trigger a pass within the settle time, not after the 60 s pause";
}

TEST(SynchronizeTests, AnUnwatchedItemWaitsForItsPause)  // [SynchronizeTests.md#watched-run]
{
    const WatchedRun run(false);
    Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(run.appears("erste.txt", 5000));

    std::ofstream(run.source / "zweite.txt") << "2";

    EXPECT_FALSE(run.appears("zweite.txt", 4000)) << "without the option the 60 s pause applies as before";
}
