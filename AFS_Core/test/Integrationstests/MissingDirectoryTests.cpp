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
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "ScopedConfigHome.h"
#include "ScopedLoggingSetup.h"
#include "SshAccount.h"
#include "SshTest.h"
#include "StrUtil.h"
#include "SyncFileSsh.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "Logging.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;
using std::chrono::milliseconds;

class MissingDirectoryTest: public ::testing::Test {  // [MissingDirectoryTests.md#MissingDirectoryTest]
protected:
    static constexpr wchar_t ITEM_NAME[] = L"verzeichnis";
    const fs::path root_ = fs::temp_directory_path() / "autofilesync_missing_directory_test";
    const fs::path source_dir_ = root_ / "source";
    const fs::path target_dir_ = root_ / "target";
    std::optional<tu::ScopedLoggingSetup> logging_;
    std::optional<tu::ScopedConfigHome> config_home_;
    std::unique_ptr<tu::ScopedDeleteMode> delete_mode_;

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        config_home_.emplace(root_ / "config");
        logging_.emplace();
        Logging::instance().set_log_file_path_name((root_ / "test.log").wstring());
        Config::instance().reset();
        delete_mode_ = std::make_unique<tu::ScopedDeleteMode>(DeleteMode::Permanent);
    }
    void TearDown() override
    {
        Synchronize::destroy();
        delete_mode_.reset();
        Config::instance().reset();
        config_home_.reset();
        logging_.reset();
        fs::remove_all(root_);
    }
    SyncItem item(SyncItemType type) const
    {
        SyncItem si(ITEM_NAME, type, source_dir_.wstring(), target_dir_.wstring(), L"");
        si.sync_cycle(0);
        return si;
    }
    SyncItem two_way_item(SyncItemType type) const
    {
        SyncItem si = item(type);
        si.two_way_sync(true);
        return si;
    }
    void write_three_source_files() const
    {
        tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
        tu::write_text_file(source_dir_ / "b.txt", "b", seconds(3600));
        tu::write_text_file(source_dir_ / "c.txt", "c", seconds(3600));
    }
    bool three_files_in(const fs::path& dir) const
    {
        return fs::exists(dir / "a.txt") && fs::exists(dir / "b.txt") && fs::exists(dir / "c.txt");
    }
    LastRun run_pass() const  // [MissingDirectoryTests.md#run_pass]
    {
        const Synchronize* sync = Synchronize::create(SyncType::Single);
        for (int i = 0; i < 1000 && !sync->finished(); ++i)
            std::this_thread::sleep_for(milliseconds(10));
        Synchronize::destroy();
        return Config::instance().syncitems()[0].last_run().value_or(LastRun{});
    }
    std::optional<SyncState> stored_state() const
    {
        return read_sync_state(sync_state_file_path(sync_state_dir(), ITEM_NAME), source_dir_.wstring(), target_dir_.wstring());
    }
};

}

TEST_F(MissingDirectoryTest, MissingTargetDirectoryIsCreatedAndFilled)  // [MissingDirectoryTests.md#MissingTargetDirectoryIsCreatedAndFilled]
{
    write_three_source_files();
    tu::write_text_file(source_dir_ / "sub" / "d.txt", "d");
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        fs::remove_all(target_dir_);
        Config::instance().reset();
        Config::instance().add(item(type));

        const LastRun run = run_pass();

        EXPECT_TRUE(run.reachable) << static_cast<int>(type);
        EXPECT_EQ(run.errors, 0u) << static_cast<int>(type);
        EXPECT_TRUE(three_files_in(target_dir_)) << static_cast<int>(type);
        EXPECT_EQ(fs::exists(target_dir_ / "sub" / "d.txt"), type == SyncItemType::Tree);
    }
}

TEST_F(MissingDirectoryTest, MissingTargetFileOfAFileItemIsCopied)
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    fs::create_directories(target_dir_);
    SyncItem si = item(SyncItemType::File);
    si.source((source_dir_ / "a.txt").wstring());
    si.target((target_dir_ / "a.txt").wstring());
    Config::instance().add(si);

    const LastRun run = run_pass();

    EXPECT_TRUE(run.reachable);
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "a");
}

#ifdef _UNIX
TEST_F(MissingDirectoryTest, TargetBelowAMissingDirectoryIsUnreachableAndCreatesNothing)  // [MissingDirectoryTests.md#TargetBelowAMissingDirectoryIsUnreachableAndCreatesNothing]
{
    write_three_source_files();
    SyncItem si = item(SyncItemType::Tree);
    si.target((root_ / "fehlt" / "target").wstring());
    Config::instance().add(si);

    const LastRun run = run_pass();

    EXPECT_FALSE(run.reachable);
    EXPECT_FALSE(fs::exists(root_ / "fehlt"));
}
#else
TEST_F(MissingDirectoryTest, TargetBelowAMissingDirectoryIsCreatedOnAnExistingDrive)  // [MissingDirectoryTests.md#TargetBelowAMissingDirectoryIsCreatedOnAnExistingDrive]
{
    write_three_source_files();
    SyncItem si = item(SyncItemType::Tree);
    si.target((root_ / "fehlt" / "target").wstring());
    Config::instance().add(si);

    const LastRun run = run_pass();

    EXPECT_TRUE(run.reachable);
    EXPECT_TRUE(three_files_in(root_ / "fehlt" / "target"));
}
#endif

TEST_F(MissingDirectoryTest, MissingSourceDirectoryDeletesNothingInTheTarget)  // [MissingDirectoryTests.md#MissingSourceDirectoryDeletesNothingInTheTarget]
{
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        fs::remove_all(source_dir_);
        tu::write_text_file(target_dir_ / "a.txt", "wertvoll");
        tu::write_text_file(target_dir_ / "sub" / "b.txt", "wertvoll");
        SyncItem si = item(type);
        si.delete_in_target(true);
        Config::instance().reset();
        Config::instance().add(si);

        const LastRun run = run_pass();

        EXPECT_FALSE(run.reachable) << static_cast<int>(type);
        EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "wertvoll") << static_cast<int>(type);
        EXPECT_EQ(tu::read_text_file(target_dir_ / "sub" / "b.txt"), "wertvoll") << static_cast<int>(type);
        EXPECT_FALSE(fs::exists(source_dir_)) << "the pass must not create the source it misses";
    }
}

TEST_F(MissingDirectoryTest, TwoWayKeepsTheSourceWhenTheTargetDirectoryVanished)  // [MissingDirectoryTests.md#vanished-target]
{
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        fs::remove_all(target_dir_);
        write_three_source_files();
        Config::instance().reset();
        Config::instance().add(two_way_item(type));
        ASSERT_EQ(run_pass().known_files, 3u) << static_cast<int>(type);
        ASSERT_TRUE(three_files_in(target_dir_));
        fs::remove_all(target_dir_);

        const LastRun run = run_pass();

        EXPECT_TRUE(three_files_in(source_dir_)) << "a vanished target directory is no deletion of its files: " << static_cast<int>(type);
        EXPECT_TRUE(three_files_in(target_dir_)) << "the target is filled again: " << static_cast<int>(type);
        EXPECT_TRUE(run.reachable);
        EXPECT_EQ(run.known_files, 3u);
    }
}

TEST_F(MissingDirectoryTest, TwoWayStillFollowsDeletionsAfterTheTargetCameBack)  // [MissingDirectoryTests.md#vanished-target]
{
    write_three_source_files();
    Config::instance().add(two_way_item(SyncItemType::Tree));
    run_pass();
    fs::remove_all(target_dir_);
    run_pass();
    ASSERT_TRUE(three_files_in(target_dir_));
    fs::remove(target_dir_ / "a.txt");

    run_pass();

    EXPECT_FALSE(fs::exists(source_dir_ / "a.txt")) << "the state written after the refill must work like any other";
    EXPECT_TRUE(fs::exists(source_dir_ / "b.txt"));
}

TEST_F(MissingDirectoryTest, TwoWayKeepsTheTargetWhenTheSourceDirectoryVanished)  // [MissingDirectoryTests.md#TwoWayKeepsTheTargetWhenTheSourceDirectoryVanished]
{
    write_three_source_files();
    Config::instance().add(two_way_item(SyncItemType::Tree));
    ASSERT_EQ(run_pass().known_files, 3u);
    fs::remove_all(source_dir_);

    const LastRun run = run_pass();

    EXPECT_FALSE(run.reachable);
    EXPECT_TRUE(three_files_in(target_dir_));
    ASSERT_TRUE(stored_state().has_value());
    EXPECT_EQ(stored_state()->files().size(), 3u) << "an unreachable side must not make the state forget its files";
}

TEST_F(MissingDirectoryTest, TwoWayFileItemKeepsTheSourceWhenTheTargetDirectoryVanished)  // [MissingDirectoryTests.md#vanished-target]
{
    tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
    fs::create_directories(target_dir_);
    SyncItem si = two_way_item(SyncItemType::File);
    si.source((source_dir_ / "a.txt").wstring());
    si.target((target_dir_ / "a.txt").wstring());
    Config::instance().add(si);
    run_pass();
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt"));
    fs::remove_all(target_dir_);

    run_pass();

    EXPECT_TRUE(fs::exists(source_dir_ / "a.txt")) << "the directory of the target file is gone, not only the file";
}

TEST_F(MissingDirectoryTest, TwoWayFileItemIntoADirectoryKeepsTheSourceWhenThatDirectoryVanished)  // [MissingDirectoryTests.md#vanished-target]
{
    tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
    fs::create_directories(target_dir_);
    SyncItem si = two_way_item(SyncItemType::File);
    si.source((source_dir_ / "a.txt").wstring());
    Config::instance().add(si);
    run_pass();
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt"));
    fs::remove_all(target_dir_);

    run_pass();

    EXPECT_TRUE(fs::exists(source_dir_ / "a.txt"));
}

TEST_F(MissingDirectoryTest, TwoWayFileDeletedInTheTargetIsStillDeletedInTheSource)  // [MissingDirectoryTests.md#TwoWayDirectoryDeletedBelowTheRootIsStillDeletedOnTheOtherSide]
{
    tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
    fs::create_directories(target_dir_);
    SyncItem si = two_way_item(SyncItemType::File);
    si.source((source_dir_ / "a.txt").wstring());
    si.target((target_dir_ / "a.txt").wstring());
    Config::instance().add(si);
    run_pass();
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt"));
    fs::remove(target_dir_ / "a.txt");

    run_pass();

    EXPECT_FALSE(fs::exists(source_dir_ / "a.txt")) << "the file is gone from a directory that is there: a deletion";
}

TEST_F(MissingDirectoryTest, TwoWayDirectoryDeletedBelowTheRootIsStillDeletedOnTheOtherSide)  // [MissingDirectoryTests.md#TwoWayDirectoryDeletedBelowTheRootIsStillDeletedOnTheOtherSide]
{
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "a", seconds(3600));
    Config::instance().add(two_way_item(SyncItemType::Tree));
    ASSERT_EQ(run_pass().known_files, 2u);
    fs::remove_all(target_dir_ / "sub");

    const LastRun run = run_pass();

    EXPECT_EQ(run.errors, 0u);
    EXPECT_FALSE(fs::exists(source_dir_ / "sub"));
    EXPECT_TRUE(fs::exists(source_dir_ / "keep.txt"));
}

TEST_F(MissingDirectoryTest, TargetThatIsAFileIsAnErrorAndStaysUntouched)  // [MissingDirectoryTests.md#TargetThatIsAFileIsAnErrorAndStaysUntouched]
{
    write_three_source_files();
    tu::write_text_file(target_dir_, "eine Datei, kein Verzeichnis");
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        SyncItem si = item(type);
        si.delete_in_target(true);
        Config::instance().reset();
        Config::instance().add(si);

        const LastRun run = run_pass();

        EXPECT_GT(run.errors, 0u) << static_cast<int>(type);
        EXPECT_EQ(tu::read_text_file(target_dir_), "eine Datei, kein Verzeichnis") << static_cast<int>(type);
        EXPECT_TRUE(three_files_in(source_dir_)) << static_cast<int>(type);
    }
}

TEST_F(MissingDirectoryTest, TwoWayKeepsTheSourceWhenTheRemoteTargetDirectoryVanished)  // [MissingDirectoryTests.md#remote-target]
{
    SKIP_WITHOUT_SSH_SERVER();
    SshAccount account;
    account.set_name("localhost");
    account.set_user(SSHTest::SSH_USER);
    account.set_host(SSHTest::SSH_HOST);
    account.set_password(SSHTest::SSH_PASSWORD);
    Config::instance().add(account);
    const std::string remote_dir = tu::concat_path(tu::test_temp_dir(), "afx_missing_directory");
    const std::wstring remote = util::string_to_wstring("localhost:" + remote_dir);
    const auto remove_remote = [&] {
        std::error_code ec;
        SSHTest::remove_remote_tree(remote_dir);
        fs::remove_all(remote_dir, ec);
    };
    remove_remote();
    write_three_source_files();
    SyncItem si = two_way_item(SyncItemType::Tree);
    si.target(remote);
    Config::instance().add(si);
    const LastRun first = run_pass();
    const bool uploaded = ssh_exists(remote + L"/a.txt");
    remove_remote();
    const bool removed = !ssh_exists(remote);

    const LastRun run = run_pass();

    const bool refilled = ssh_exists(remote + L"/a.txt") && ssh_exists(remote + L"/c.txt");
    remove_remote();
    ASSERT_EQ(first.known_files, 3u);
    ASSERT_TRUE(uploaded);
    ASSERT_TRUE(removed);
    EXPECT_TRUE(three_files_in(source_dir_)) << "a vanished remote directory is no deletion of its files";
    EXPECT_TRUE(refilled);
    EXPECT_EQ(run.known_files, 3u);
}

#ifndef _WIN32
TEST_F(MissingDirectoryTest, UnreadableSourceDirectoryDeletesNothingInTheTarget)  // [MissingDirectoryTests.md#UnreadableSourceDirectoryDeletesNothingInTheTarget]
{
    if (geteuid() == 0)
        GTEST_SKIP() << "root reads every directory";
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "a", seconds(3600));
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    for (const bool two_way : {false, true}) {
        fs::remove_all(target_dir_);
        SyncItem si = two_way ? two_way_item(SyncItemType::Tree) : item(SyncItemType::Tree);
        si.delete_in_target(!two_way);
        Config::instance().reset();
        Config::instance().add(si);
        run_pass();
        ASSERT_TRUE(fs::exists(target_dir_ / "sub" / "a.txt")) << two_way;
        fs::permissions(source_dir_ / "sub", fs::perms::none);

        const LastRun run = run_pass();

        fs::permissions(source_dir_ / "sub", fs::perms::owner_all);
        EXPECT_GT(run.errors, 0u) << two_way;
        EXPECT_TRUE(fs::exists(target_dir_ / "sub" / "a.txt")) << "what cannot be read is not gone: " << two_way;
        EXPECT_TRUE(fs::exists(source_dir_ / "sub" / "a.txt")) << two_way;
        EXPECT_TRUE(fs::exists(target_dir_ / "keep.txt")) << two_way;
    }
}
#endif
