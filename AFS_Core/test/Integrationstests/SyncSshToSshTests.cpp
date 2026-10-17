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
#include "FileUtil.h"
#include "SshTest.h"
#include "SyncFileSsh.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Util.h"
#include <chrono>
#include <filesystem>
#include <thread>

using namespace AFS_Core;
using namespace SSHTest;

namespace {

namespace fs = std::filesystem;
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

CompareItems compare_by_time()  // [SyncSshToSshTests.md#compare_by_time]
{
    CompareItems ci;
    ci.content(false);
    ci.time(true);
    return ci;
}

class SyncSshToSshTest : public ::testing::Test {  // [SyncSshToSshTests.md#SyncSshToSshTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / "afs_ssh_to_ssh" / ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        Config::instance().add(test_account());
        fs::remove_all(root_);
        for (const fs::path& dir : {root_.parent_path(), root_, source_, target_}) {
            fs::create_directories(dir);
            fs::permissions(dir, fs::perms::all);  // [SyncSshToSshTests.md#both-sides-on-disk]
        }
        Synchronize::start();
    }

    void TearDown() override
    {
        Synchronize::start();
        SSHTest::remove_remote_tree(target_.string());  // [SyncSshToSshTests.md#clean-up-as-the-ssh-user]
        std::error_code ec;
        fs::remove_all(root_.parent_path(), ec);
    }

    static std::wstring remote(const fs::path& path)
    {
        return string_to_wstring(ACCOUNT_NAME + ":" + path.string());
    }

    void write_source(const char* name, const std::string& content, std::chrono::seconds age = std::chrono::seconds(0)) const
    {
        tu::write_text_file(source_ / name, content, age);
        fs::permissions(source_ / name, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    }

    SyncItem item(SyncItemType type) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(remote(source_));
        si.target(remote(target_));
        si.compare_items(compare_by_time());
        return si;
    }

    SyncItem file_item(const char* name) const
    {
        SyncItem si = item(SyncItemType::File);
        si.source(remote(source_ / name));
        si.target(remote(target_ / name));
        return si;
    }

    static std::time_t whole_seconds(const fs::path& file)
    {
        return util::to_time_t(fs::last_write_time(file));
    }
};

} // namespace

TEST_F(SyncSshToSshTest, CopyKeepsContentTimeAndPermissions)  // [SyncSshToSshTests.md#CopyKeepsContentTimeAndPermissions]
{
    write_source("a.txt", "Inhalt", std::chrono::seconds(3600));
    fs::permissions(source_ / "a.txt", fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read);

    EXPECT_EQ(copy_ssh_to_ssh(remote(source_ / "a.txt"), remote(target_ / "a.txt")), CopyResult::OK);

    EXPECT_EQ(tu::read_text_file(target_ / "a.txt"), "Inhalt");
    EXPECT_EQ(whole_seconds(target_ / "a.txt"), whole_seconds(source_ / "a.txt")) << "die Aenderungszeit muss mitkommen, sonst kopiert der naechste Lauf erneut";
    EXPECT_EQ(fs::status(target_ / "a.txt").permissions(), fs::status(source_ / "a.txt").permissions());
}

TEST_F(SyncSshToSshTest, MissingSourceIsAnErrorAndCreatesNoTarget)
{
    EXPECT_EQ(copy_ssh_to_ssh(remote(source_ / "fehlt.txt"), remote(target_ / "fehlt.txt")), CopyResult::Error);

    EXPECT_FALSE(fs::exists(target_ / "fehlt.txt"));
}

TEST_F(SyncSshToSshTest, FileItemBetweenTwoSshPathsIsCopiedOnce)  // [SyncSshToSshTests.md#FileItemBetweenTwoSshPathsIsCopiedOnce]
{
    write_source("a.txt", "Inhalt", std::chrono::seconds(3600));

    EXPECT_EQ(tu::run_sync(file_item("a.txt")), 1u);
    EXPECT_EQ(tu::read_text_file(target_ / "a.txt"), "Inhalt");

    EXPECT_EQ(tu::run_sync(file_item("a.txt")), 0u) << "nach der Kopie sind beide Seiten gleich";
}

TEST_F(SyncSshToSshTest, DirItemCopiesEveryFileAndDeletesTargetOnlyFiles)  // [SyncSshToSshTests.md#DirItemCopiesEveryFileAndDeletesTargetOnlyFiles]
{
    write_source("a.txt", "a", std::chrono::seconds(3600));
    write_source("b.txt", "b", std::chrono::seconds(3600));
    tu::write_text_file(target_ / "verwaist.txt", "weg");
    fs::permissions(target_ / "verwaist.txt", fs::perms::all);
    SyncItem si = item(SyncItemType::Dir);
    si.delete_in_target(true);

    EXPECT_EQ(tu::run_sync(si), 2u);

    EXPECT_EQ(tu::read_text_file(target_ / "a.txt"), "a");
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b");
    EXPECT_FALSE(fs::exists(target_ / "verwaist.txt")) << "die Nur-im-Ziel-Datei muss ueber SFTP geloescht werden";
}

TEST_F(SyncSshToSshTest, TreeItemCreatesTheNestedDirectories)  // [SyncSshToSshTests.md#TreeItemCreatesTheNestedDirectories]
{
    fs::create_directories(source_ / "sub" / "tief");
    fs::permissions(source_ / "sub", fs::perms::all);
    fs::permissions(source_ / "sub" / "tief", fs::perms::all);
    write_source("sub/tief/c.txt", "c", std::chrono::seconds(3600));
    write_source("oben.txt", "o", std::chrono::seconds(3600));

    EXPECT_EQ(tu::run_sync(item(SyncItemType::Tree)), 2u);

    EXPECT_EQ(tu::read_text_file(target_ / "oben.txt"), "o");
    EXPECT_EQ(tu::read_text_file(target_ / "sub" / "tief" / "c.txt"), "c");
}

TEST_F(SyncSshToSshTest, TwoWaySyncCopiesTargetOnlyFilesBack)  // [SyncSshToSshTests.md#TwoWaySyncCopiesTargetOnlyFilesBack]
{
    write_source("hin.txt", "hin", std::chrono::seconds(3600));
    tu::write_text_file(target_ / "zurueck.txt", "zurück", std::chrono::seconds(3600));
    fs::permissions(target_ / "zurueck.txt", fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    SyncItem si = item(SyncItemType::Dir);
    si.two_way_sync(true);

    EXPECT_EQ(tu::run_sync(si), 2u);

    EXPECT_EQ(tu::read_text_file(target_ / "hin.txt"), "hin");
    EXPECT_EQ(tu::read_text_file(source_ / "zurueck.txt"), "zurück");
    EXPECT_EQ(tu::run_sync(si), 0u) << "danach ist nichts mehr zu tun";
}

TEST_F(SyncSshToSshTest, StopDuringTheCopyLeavesNoHalfFileInTheTarget)  // [SyncSshToSshTests.md#StopDuringTheCopyLeavesNoHalfFileInTheTarget]
{
    tu::write_large_file(source_ / "gross.bin", 64u << 20);
    fs::permissions(source_ / "gross.bin", fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    std::thread stopper([&] {
        while (!fs::exists(target_ / "gross.bin"))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Synchronize::stop();
    });

    const std::size_t copy_count = tu::run_sync(file_item("gross.bin"));
    stopper.join();

    EXPECT_EQ(copy_count, 0u) << "die Kopie war schon fertig, bevor der Stopp ankam - die Datei muss groesser sein";
    EXPECT_FALSE(fs::exists(target_ / "gross.bin")) << "eine halb kopierte Datei darf nicht im Ziel zurueckbleiben";
}
