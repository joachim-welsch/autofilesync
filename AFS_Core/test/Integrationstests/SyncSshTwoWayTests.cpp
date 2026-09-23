/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "pch.h"
#include "Config.h"
#include "DirUtil.h"
#include "FileUtil.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include "SshTest.h"
#include "SyncFileSsh.h"
#include "SyncRunner.h"
#include "SyncSshTwoWayTests.h"
#include "Synchronize.h"
#include "Util.h"
#include <chrono>
#include <filesystem>

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
