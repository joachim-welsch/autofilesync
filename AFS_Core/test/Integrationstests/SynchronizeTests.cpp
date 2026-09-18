#include "pch.h"
#include "Synchronize.h"
#include "Config.h"
#include "SshTest.h"
#include "FileUtil.h"
#include "DirUtil.h"
#include <Util.h>
#include <Logging.h>
#include <Translation.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>

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
#ifdef _UNIX
    EXPECT_FALSE(Synchronize::create(SyncType::Single)->path_exists(L"/home/welschj/temp/notexisting"));
#else
    EXPECT_FALSE(Synchronize::create(SyncType::Single)->path_exists(L"C:\\temp\\notexisting"));
#endif
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
    EXPECT_EQ(count_lines_containing(log, "Error\t" + message), 1);
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
TEST(SyncControlerTests, RequestStopReturnsImmediatelyUnlikeStopSync)
{
    Config::instance().reset();
    auto& sc = SyncControler::instance();
    sc.reset_state();
    ASSERT_TRUE(sc.start_sync(SyncType::Continuous));

    const auto start = std::chrono::steady_clock::now();
    sc.request_stop();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsed_ms, 200) << "requestStop() must signal and return, not wait for the worker to finish";

    sc.destroy_synchronize();  // [SynchronizeTests.md#cleanup-via-destroy]
    sc.reset_state();
    Config::instance().reset();
}
