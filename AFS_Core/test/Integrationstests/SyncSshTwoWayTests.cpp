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
#include "Config.h"
#include "DirUtil.h"
#include "ScopedDeleteMode.h"
#include "FileUtil.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include "SftpConnection.h"
#include "SshTest.h"
#include "SyncFileSsh.h"
#include "SyncRunner.h"
#include "SyncTree.h"
#include "SyncSshTwoWayTests.h"
#include "Synchronize.h"
#include "Util.h"
#include <chrono>
#include <filesystem>
#include <thread>

using namespace AFS_Core;
using namespace SSHTest;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;
using util::string_to_wstring;

const std::string ACCOUNT_NAME = "localhost";

SshAccount test_account()
{
    SshAccount account;
    account.set_name(ACCOUNT_NAME);
    account.set_user(SSH_USER);
    account.set_host(SSH_HOST);
    account.set_password(SSH_PASSWORD);
    return account;
}

CompareItems compare_by_time()  // [SyncSshTwoWayTests.md#compare_by_time]
{
    CompareItems ci;
    ci.content(false);
    ci.time(true);
    return ci;
}

class SyncSshTwoWayTest: public ::testing::Test {
protected:
    const fs::path source_dir_{SyncSshTwoWayTests::SOURCE_PATH};
    const fs::path target_dir_{SyncSshTwoWayTests::TARGET_PATH};
    const std::string remote_base_dir_ = tu::concat_path(tu::test_temp_dir(), SyncSshTwoWayTests::REMOTE_DIR_NAME);
    std::string remote_dir_;
    std::wstring remote_dir_w_;

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        Config::instance().add(test_account());
        remote_dir_ = tu::concat_path(remote_base_dir_, ::testing::UnitTest::GetInstance()->current_test_info()->name());  // [SyncSshTwoWayTests.md#remote-dir-per-test]
        remote_dir_w_ = string_to_wstring(ACCOUNT_NAME + ":" + remote_dir_);
        clean_up();
        create_remote_dirs();
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override { clean_up(); }

    void clean_up() const
    {
        std::error_code ec;
        fs::remove_all(source_dir_, ec);
        fs::remove_all(target_dir_, ec);
        if (!remote_dir_w_.empty())
            tu::erase_ssh_test_dir(remote_dir_w_);
    }

    void create_remote_dirs() const  // [SyncSshTwoWayTests.md#create_remote_dirs]
    {
        for (const std::wstring& dir : { string_to_wstring(ACCOUNT_NAME + ":" + remote_base_dir_), remote_dir_w_ })
            if (!ssh_exists(dir))
                ssh_mkdir(dir);
    }

    bool remote_exists(const std::string& name) const
    {
        SshFileInspector inspector(test_account());
        return inspector.inspect_file(tu::concat_path(remote_dir_, name)).exists;
    }

    std::time_t remote_time(const std::string& name) const
    {
        SshFileInspector inspector(test_account());
        return inspector.inspect_file(tu::concat_path(remote_dir_, name)).raw_mtime;
    }

    static std::time_t local_time(const fs::path& file)  // [SyncSshTwoWayTests.md#whole-seconds]
    {
        return util::to_time_t(fs::last_write_time(file));
    }

    std::wstring remote_path(const std::wstring& name) const
    {
        return remote_dir_w_ + L"/" + name;
    }

    SyncItem ssh_target_item(SyncItemType type, bool two_way) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source_dir_.wstring());
        si.target(remote_dir_w_);
        si.two_way_sync(two_way);
        return si;
    }

    SyncItem ssh_source_item(SyncItemType type, bool two_way) const  // [SyncSshTwoWayTests.md#ssh_source_item]
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(remote_dir_w_);
        si.target(target_dir_.wstring());
        si.two_way_sync(two_way);
        return si;
    }

    SyncItem ssh_target_file_item(const std::wstring& filename, bool two_way) const
    {
        SyncItem si = ssh_target_item(SyncItemType::File, two_way);
        si.source((source_dir_ / filename).wstring());
        si.target(remote_path(filename));
        return si;
    }

    void seed_remote(SyncItemType type = SyncItemType::Dir) const  // [SyncSshTwoWayTests.md#seed_remote]
    {
        tu::run_sync(ssh_target_item(type, false));
    }
};

TEST_F(SyncSshTwoWayTest, RemoteTargetOnlyFileIsCopiedBackToTheLocalSource)
{
    tu::write_text_file(source_dir_ / "from_remote.txt", "created on the remote side");
    seed_remote();
    ASSERT_TRUE(remote_exists("from_remote.txt"));
    fs::remove(source_dir_ / "from_remote.txt");

    EXPECT_EQ(tu::run_sync(ssh_target_item(SyncItemType::Dir, true)), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "from_remote.txt"), "created on the remote side");
    EXPECT_TRUE(remote_exists("from_remote.txt")) << "two-way sync must not remove the remote original";
}

TEST_F(SyncSshTwoWayTest, MissingLocalSourceFileIsRestoredFromTheRemoteTarget)
{
    tu::write_text_file(source_dir_ / "notes.txt", "kept on the remote side");
    seed_remote();
    ASSERT_TRUE(remote_exists("notes.txt"));
    fs::remove(source_dir_ / "notes.txt");

    EXPECT_EQ(tu::run_sync(ssh_target_file_item(L"notes.txt", true)), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "kept on the remote side");
}

TEST_F(SyncSshTwoWayTest, NewerRemoteFileIsCopiedBackToTheLocalSource)
{
    tu::write_text_file(source_dir_ / "notes.txt", "edited on the remote side");
    seed_remote();
    tu::write_text_file(source_dir_ / "notes.txt", "older local version", seconds(3600));  // [SyncSshTwoWayTests.md#remote-time-is-the-transfer-time]

    SyncItem si = ssh_target_file_item(L"notes.txt", true);
    si.compare_items(compare_by_time());
    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "edited on the remote side");
}

TEST_F(SyncSshTwoWayTest, RemoteChangeIsNoticedWithTheDefaultComparison)  // [SyncSshTwoWayTests.md#RemoteChangeIsNoticedWithTheDefaultComparison]
{
    tu::write_text_file(source_dir_ / "notes.txt", "edited on the remote side");
    seed_remote();
    tu::write_text_file(source_dir_ / "notes.txt", "older", seconds(3600));

    tu::run_sync(ssh_target_file_item(L"notes.txt", true));

    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "edited on the remote side")
        << "content comparison cannot run over SSH, so the comparison must fall back to size and time";
}

TEST_F(SyncSshTwoWayTest, LocalTargetOnlyFileIsCopiedBackToTheRemoteSource)  // [SyncSshTwoWayTests.md#ssh_source_item]
{
    tu::write_text_file(source_dir_ / "on_the_remote.txt", "created on the remote side");
    seed_remote();
    tu::write_text_file(target_dir_ / "only_local.txt", "created on the local side");

    tu::run_sync(ssh_source_item(SyncItemType::Dir, true));

    EXPECT_TRUE(remote_exists("only_local.txt")) << "the local-only file must reach the remote source";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "on_the_remote.txt"), "created on the remote side");
}

TEST_F(SyncSshTwoWayTest, DeleteInTargetWinsOverTwoWaySyncOnTheRemoteTarget)
{
    tu::write_text_file(source_dir_ / "orphan.txt", "only on the remote side");
    seed_remote();
    ASSERT_TRUE(remote_exists("orphan.txt"));
    fs::remove(source_dir_ / "orphan.txt");

    SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    si.delete_in_target(true);
    tu::run_sync(si);

    EXPECT_FALSE(remote_exists("orphan.txt"));
    EXPECT_FALSE(fs::exists(source_dir_ / "orphan.txt")) << "the deleted file must not come back through the two-way direction";
}

TEST_F(SyncSshTwoWayTest, TreeSyncCopiesARemoteOnlyFileFromANestedLevelBack)
{
    tu::write_text_file(source_dir_ / "sub" / "nested.txt", "created on the remote side");
    seed_remote(SyncItemType::Tree);
    ASSERT_TRUE(remote_exists("sub/nested.txt"));
    fs::remove(source_dir_ / "sub" / "nested.txt");

    EXPECT_EQ(tu::run_sync(ssh_target_item(SyncItemType::Tree, true)), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "sub" / "nested.txt"), "created on the remote side");
}

TEST_F(SyncSshTwoWayTest, UploadKeepsTheModificationTime)  // [SyncSshTwoWayTests.md#modification-time-survives-the-transfer]
{
    tu::write_text_file(source_dir_ / "notes.txt", "created on the local side", seconds(3600));
    seed_remote();

    EXPECT_EQ(remote_time("notes.txt"), local_time(source_dir_ / "notes.txt"));
}

TEST_F(SyncSshTwoWayTest, DownloadKeepsTheModificationTime)  // [SyncSshTwoWayTests.md#modification-time-survives-the-transfer]
{
    tu::write_text_file(source_dir_ / "notes.txt", "created on the remote side", seconds(3600));
    seed_remote();
    const std::time_t uploaded = remote_time("notes.txt");
    fs::remove(source_dir_ / "notes.txt");

    tu::run_sync(ssh_target_item(SyncItemType::Dir, true));

    ASSERT_TRUE(fs::exists(source_dir_ / "notes.txt"));
    EXPECT_EQ(local_time(source_dir_ / "notes.txt"), uploaded);
}

TEST_F(SyncSshTwoWayTest, RepeatedSshSyncWithTimeComparisonCopiesNothingMore)  // [SyncSshTwoWayTests.md#idempotence]
{
    tu::write_text_file(source_dir_ / "notes.txt", "created on the local side");

    SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    si.compare_items(compare_by_time());
    EXPECT_EQ(tu::run_sync(si), 1u);
    EXPECT_EQ(tu::run_sync(si), 0u) << "the uploaded copy must not look newer than the original it came from";
}

TEST_F(SyncSshTwoWayTest, RepeatedSshSyncCopiesNothingMore)  // [SyncSshTwoWayTests.md#idempotence]
{
    tu::write_text_file(source_dir_ / "notes.txt", "created on the local side");

    const SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    EXPECT_EQ(tu::run_sync(si), 1u);
    EXPECT_EQ(tu::run_sync(si), 0u) << "a settled pair of directories must not keep copying files back and forth";
}

} // namespace

TEST_F(SyncSshTwoWayTest, LocalDeletionReachesTheRemoteTargetWhenTheStateKnowsIt)  // [SyncSshTwoWayTests.md#state]
{
    tu::write_text_file(source_dir_ / "gone.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "kept.txt", "stays", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    ASSERT_TRUE(remote_exists("gone.txt"));
    EXPECT_EQ(state.files().size(), 2u);
    fs::remove(source_dir_ / "gone.txt");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(remote_exists("gone.txt")) << "the deletion must reach the server instead of being undone from there";
    EXPECT_TRUE(remote_exists("kept.txt"));
    EXPECT_EQ(state.find(L"gone.txt"), nullptr);
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(SyncSshTwoWayTest, RemoteDeletionReachesTheLocalSourceWhenTheStateKnowsIt)  // [SyncSshTwoWayTests.md#state]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "gone.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "kept.txt", "stays", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    ASSERT_TRUE(ssh_remove_file(remote_path(L"gone.txt")));

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(source_dir_ / "gone.txt")) << "a deletion on the server must reach the local side";
    EXPECT_TRUE(fs::exists(source_dir_ / "kept.txt"));
    EXPECT_TRUE(remote_exists("kept.txt"));
}

TEST_F(SyncSshTwoWayTest, RemoteEditAfterLocalDeletionIsCopiedBackNotDeleted)
{
    tu::write_text_file(source_dir_ / "notes.txt", "v1", seconds(7200));
    const SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    fs::remove(source_dir_ / "notes.txt");
    tu::write_text_file(target_dir_ / "notes.txt", "edited on the server");
    ASSERT_TRUE(ssh_remove_file(remote_path(L"notes.txt")));
    tu::run_sync([&] { SyncItem upload = ssh_target_file_item(L"notes.txt", false); upload.source((target_dir_ / "notes.txt").wstring()); return upload; }());

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "edited on the server");
}

TEST_F(SyncSshTwoWayTest, RemoteOnlyDirectoryIsMirroredLocally)  // [SyncSshTwoWayTests.md#directories]
{
    tu::write_text_file(source_dir_ / "remote_only" / "inside.txt", "created on the remote side");
    seed_remote(SyncItemType::Tree);
    ASSERT_TRUE(remote_exists("remote_only/inside.txt"));
    fs::remove_all(source_dir_ / "remote_only");

    EXPECT_EQ(tu::run_sync(ssh_target_item(SyncItemType::Tree, true)), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "remote_only" / "inside.txt"), "created on the remote side");
    EXPECT_TRUE(remote_exists("remote_only/inside.txt"));
}

TEST_F(SyncSshTwoWayTest, LocalDirectoryDeletionReachesTheRemoteTargetWhenTheStateKnowsIt)  // [SyncSshTwoWayTests.md#directories]
{
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Tree, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    ASSERT_TRUE(remote_exists("sub/a.txt"));
    fs::remove_all(source_dir_ / "sub");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(remote_exists("sub/a.txt"));
    EXPECT_FALSE(ssh_exists(remote_path(L"sub"))) << "the emptied remote directory must go too";
    EXPECT_TRUE(remote_exists("keep.txt"));
    EXPECT_FALSE(state.knows_below(L"sub"));
}

TEST_F(SyncSshTwoWayTest, RemoteDirectoryDeletionReachesTheLocalSourceWhenTheStateKnowsIt)  // [SyncSshTwoWayTests.md#directories]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Tree, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    ASSERT_TRUE(ssh_remove_file(remote_path(L"sub/a.txt")));
    ASSERT_TRUE(ssh_rmdir(remote_path(L"sub")));

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(source_dir_ / "sub")) << "a directory deleted on the server must leave the local side";
    EXPECT_FALSE(ssh_exists(remote_path(L"sub"))) << "the pass must not recreate the deleted directory on the server";
    EXPECT_TRUE(fs::exists(source_dir_ / "keep.txt"));
}

TEST_F(SyncSshTwoWayTest, RenamedLocalFileIsRenamedOnTheServer)  // [SyncSshTwoWayTests.md#renames]
{
    tu::write_text_file(source_dir_ / "old.txt", "renamed, not uploaded again", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Tree, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    const std::time_t uploaded = remote_time("old.txt");
    fs::create_directories(source_dir_ / "sub");
    fs::rename(source_dir_ / "old.txt", source_dir_ / "sub" / "new.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_TRUE(remote_exists("sub/new.txt"));
    EXPECT_FALSE(remote_exists("old.txt"));
    EXPECT_EQ(remote_time("sub/new.txt"), uploaded);
    EXPECT_NE(state.find(L"sub/new.txt"), nullptr);
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(SyncSshTwoWayTest, ConflictWithTheServerKeepsBothVersions)  // [SyncSshTwoWayTests.md#conflict-copies]
{
    tu::write_text_file(source_dir_ / "notes.txt", "v1", seconds(7200));
    const SyncItem si = ssh_target_item(SyncItemType::Dir, true);
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    tu::write_text_file(target_dir_ / "notes.txt", "server v2", seconds(3600));
    ASSERT_TRUE(ssh_remove_file(remote_path(L"notes.txt")));
    tu::run_sync([&] { SyncItem upload = ssh_target_file_item(L"notes.txt", false); upload.source((target_dir_ / "notes.txt").wstring()); return upload; }());
    tu::write_text_file(source_dir_ / "notes.txt", "local v2, written last");

    EXPECT_EQ(tu::run_sync(si, state), 2u);

    EXPECT_EQ(state.files().size(), 2u) << "the file and its conflict copy are both known";
    std::size_t copies = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(source_dir_))
        if (entry.path().filename().string().find("notes (") == 0) {
            ++copies;
            EXPECT_EQ(tu::read_text_file(entry.path()), "server v2") << "the server's version survives as the conflict copy";
            EXPECT_NE(entry.path().filename().string().find(SSH_HOST), std::string::npos) << "the copy names the server whose version lost: " << entry.path().filename().string();
            EXPECT_TRUE(remote_exists(entry.path().filename().string())) << "the conflict copy reaches the server too";
        }
    EXPECT_EQ(copies, 1u);
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(SyncSshTwoWayTest, APassSharesOneSessionPerAccount)  // [SyncSshTwoWayTests.md#sessions]
{
    for (int dir = 0; dir < 3; ++dir)
        for (int i = 0; i < 4; ++i)
            tu::write_text_file(source_dir_ / ("ordner" + std::to_string(dir)) / ("datei" + std::to_string(i) + ".txt"), "x", seconds(3600));
    const SyncItem si = ssh_target_item(SyncItemType::Tree, false);
    using namespace std::chrono;

    sftp_connect_count = 0;
    const auto start = steady_clock::now();
    EXPECT_EQ(tu::run_sync(si), 12u);
    const auto shared = steady_clock::now();
    EXPECT_EQ(sftp_connect_count.load(), 1) << "one login for the whole pass";
    EXPECT_TRUE(remote_exists("ordner2/datei3.txt"));

    sftp_connect_count = 0;
    std::size_t count = 0;
    SyncTree unscoped(si, count, false);
    unscoped();
    const auto separate = steady_clock::now();
    EXPECT_EQ(count, 0u);
    EXPECT_GT(sftp_connect_count.load(), 12) << "without a scope every operation logs in, as before";
    std::cout << "one session: " << duration_cast<milliseconds>(shared - start).count() << " ms for 12 uploads, a session per operation: "
        << duration_cast<milliseconds>(separate - shared).count() << " ms for the quiet pass after it (" << sftp_connect_count.load() << " logins)\n";
}

TEST_F(SyncSshTwoWayTest, SessionsEndWithTheScope)  // [SyncSshTwoWayTests.md#sessions]
{
    sftp_connect_count = 0;
    {
        SshSessionScope sessions;
        EXPECT_TRUE(SshSessionScope::active());
        EXPECT_TRUE(ssh_exists(remote_dir_w_));
        EXPECT_TRUE(ssh_is_directory(remote_dir_w_));
        {
            SshSessionScope nested;
            EXPECT_TRUE(ssh_exists(remote_dir_w_));
        }
        EXPECT_TRUE(SshSessionScope::active()) << "the inner scope must not close the outer one's sessions";
        EXPECT_TRUE(ssh_exists(remote_dir_w_));
    }
    EXPECT_EQ(sftp_connect_count.load(), 1);
    EXPECT_FALSE(SshSessionScope::active());

    EXPECT_TRUE(ssh_exists(remote_dir_w_));
    EXPECT_EQ(sftp_connect_count.load(), 2) << "after the scope a new session is opened";
}

TEST_F(SyncSshTwoWayTest, ASinglePassOfSynchronizeLogsInOnce)  // [SyncSshTwoWayTests.md#sessions]
{
    for (int i = 0; i < 5; ++i)
        tu::write_text_file(source_dir_ / ("datei" + std::to_string(i) + ".txt"), "x", seconds(3600));
    SyncItem si = ssh_target_item(SyncItemType::Tree, false);
    si.name(L"ssh session test");
    Config::instance().add(si);
    const auto before = Config::instance().find_by_name(L"ssh session test")->last_sync();

    sftp_connect_count = 0;
    Synchronize::create(SyncType::Single);
    for (int i = 0; i < 1500 && Config::instance().find_by_name(L"ssh session test")->last_sync() == before; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Synchronize::destroy();

    EXPECT_EQ(sftp_connect_count.load(), 1) << "the path check, the listing and five uploads share one login";
    EXPECT_TRUE(remote_exists("datei4.txt"));
    EXPECT_TRUE(Config::instance().erase_by_name(L"ssh session test"));
}
