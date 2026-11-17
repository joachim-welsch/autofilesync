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
#ifdef _UNIX
#include "Config.h"
#include "CopyLog.h"
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "ScopedLog.h"
#include "SshTest.h"
#include "SyncFileSsh.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Logging.h"
#include <filesystem>
#include <set>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class SymlinkSshTest : public ::testing::Test {  // [SymlinkSshTests.md#SymlinkSshTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("afs_sshlink_" + std::to_string(TestPaths::process_id()));
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";
    const tu::ScopedDeleteMode delete_mode_{DeleteMode::Permanent};

    void SetUp() override
    {
        fs::remove_all(root_);
        for (const fs::path& dir : {root_, source_, source_ / "echt", target_}) {
            fs::create_directories(dir);
            fs::permissions(dir, fs::perms::all);
        }
        tu::write_text_file(source_ / "echt" / "a.txt", "a");
        tu::write_text_file(source_ / "b.txt", "b");
        for (const char* file : {"echt/a.txt", "b.txt"})
            fs::permissions(source_ / file, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
        SshAccount account;
        account.set_name("localhost");
        account.set_user(SSHTest::SSH_USER);
        account.set_host(SSHTest::SSH_HOST);
        account.set_password(SSHTest::SSH_PASSWORD);
        Config::instance().add(account);
        Synchronize::start();
    }

    void TearDown() override
    {
        if (SSHTest::ssh_available())
            SSHTest::remove_remote_tree(root_.string());  // [SymlinkSshTests.md#teardown]
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    static std::wstring remote(const fs::path& path) { return L"localhost:" + path.wstring(); }

    SyncItem item(const std::wstring& source, const std::wstring& target) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Tree);
        si.source(source);
        si.target(target);
        si.symlink_mode(SymlinkMode::AsLink);
        return si;
    }

    SyncItem to_remote() const { return item(source_.wstring(), remote(target_)); }
    SyncItem from_remote() const { return item(remote(source_), target_.wstring()); }

    static void run(const SyncItem& si, Outcome& outcome, SyncState* state = nullptr)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }
};

} // namespace

TEST_F(SymlinkSshTest, LinkInfoOverSftpTellsLinkFileDirectoryAndNothingApart)  // [SymlinkSshTests.md#LinkInfoOverSftpTellsLinkFileDirectoryAndNothingApart]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "datei");
    fs::create_directory_symlink("echt", source_ / "ordner");
    fs::create_symlink("weg.txt", source_ / "tot");

    const std::optional<SshLink> file = ssh_link_info(remote(source_ / "datei"));
    const std::optional<SshLink> directory = ssh_link_info(remote(source_ / "ordner"));
    const std::optional<SshLink> dead = ssh_link_info(remote(source_ / "tot"));
    const std::optional<SshLink> plain = ssh_link_info(remote(source_ / "b.txt"));
    const std::optional<SshLink> real_directory = ssh_link_info(remote(source_ / "echt"));
    const std::optional<SshLink> missing = ssh_link_info(remote(source_ / "fehlt"));

    ASSERT_TRUE(file && directory && dead && plain && real_directory && missing) << "every question got an answer";
    EXPECT_TRUE(file->is_link && file->present && !file->to_directory);
    EXPECT_EQ(file->text, L"b.txt");
    EXPECT_TRUE(directory->is_link && directory->to_directory && !directory->is_directory);
    EXPECT_TRUE(dead->is_link && !dead->to_directory);
    EXPECT_EQ(dead->text, L"weg.txt");
    EXPECT_TRUE(plain->present && !plain->is_link && !plain->is_directory);
    EXPECT_TRUE(real_directory->present && !real_directory->is_link && real_directory->is_directory);
    EXPECT_FALSE(missing->present);
}

TEST_F(SymlinkSshTest, LinksAreCreatedOnAnSftpTargetWithTheirText)  // [SymlinkSshTests.md#LinksAreCreatedOnAnSftpTargetWithTheirText]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "datei.txt");
    fs::create_directory_symlink("echt", source_ / "ordner");
    fs::create_symlink(source_ / "b.txt", source_ / "echt" / "absolut.txt");
    fs::create_symlink("weg.txt", source_ / "tot.txt");
    Outcome first, second;

    run(to_remote(), first);

    EXPECT_TRUE(first.report.successful());
    EXPECT_EQ(first.copies, 6u) << "two files and four links";
    EXPECT_EQ(fs::read_symlink(target_ / "datei.txt"), fs::path("b.txt")) << "the text arrived, not the reversed pair of an old server quirk";
    EXPECT_EQ(fs::read_symlink(target_ / "ordner"), fs::path("echt"));
    EXPECT_EQ(fs::read_symlink(target_ / "echt" / "absolut.txt"), fs::path("../b.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "tot.txt"), fs::path("weg.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "ordner" / "a.txt"), "a");

    run(to_remote(), second);

    EXPECT_EQ(second.copies, 0u);
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "datei.txt", "echt", "ordner", "tot.txt"})) << "nothing prepared or parked is left";
}

TEST_F(SymlinkSshTest, ChangedLinkReplacesTheOneOnTheSftpTarget)  // [SymlinkSshTests.md#ChangedLinkReplacesTheOneOnTheSftpTarget]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "zeiger");
    Outcome first, second;
    run(to_remote(), first);
    fs::remove(source_ / "zeiger");
    fs::create_symlink("echt/a.txt", source_ / "zeiger");

    run(to_remote(), second);

    EXPECT_TRUE(second.report.successful());
    EXPECT_EQ(second.copies, 1u);
    EXPECT_EQ(fs::read_symlink(target_ / "zeiger"), fs::path("echt/a.txt"));
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt", "zeiger"}));
}

TEST_F(SymlinkSshTest, LinksOfAnSftpSourceArriveAsLinks)  // [SymlinkSshTests.md#LinksOfAnSftpSourceArriveAsLinks]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "datei.txt");
    fs::create_directory_symlink("echt", source_ / "ordner");
    fs::create_symlink(source_ / "b.txt", source_ / "absolut.txt");
    Outcome outcome;

    run(from_remote(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(outcome.copies, 5u) << "two files and three links";
    EXPECT_EQ(fs::read_symlink(target_ / "datei.txt"), fs::path("b.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "ordner"), fs::path("echt"));
    EXPECT_EQ(fs::read_symlink(target_ / "absolut.txt"), fs::path("b.txt")) << "made relative by the remote root";
    EXPECT_FALSE(fs::exists(target_ / "ordner" / "ordner"));
}

TEST_F(SymlinkSshTest, DeleteInTargetRemovesALinkOnTheSftpTarget)  // [SymlinkSshTests.md#DeleteInTargetRemovesALinkOnTheSftpTarget]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "zeiger");
    SyncItem si = to_remote();
    si.delete_in_target(true);
    Outcome first, second;
    run(si, first);
    ASSERT_TRUE(fs::is_symlink(target_ / "zeiger"));
    fs::remove(source_ / "zeiger");

    run(si, second);

    EXPECT_TRUE(second.report.successful());
    EXPECT_FALSE(fs::is_symlink(target_ / "zeiger"));
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b") << "what the link pointed to stays";
}

TEST_F(SymlinkSshTest, TwoWaySyncWithAnSftpTargetCarriesAndDeletesLinks)  // [SymlinkSshTests.md#TwoWaySyncWithAnSftpTargetCarriesAndDeletesLinks]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_symlink("b.txt", source_ / "hier");
    SyncItem si = to_remote();
    si.two_way_sync(true);
    SyncState state(si.source(), si.target());
    Outcome first, second;
    run(si, first, &state);
    ASSERT_TRUE(fs::is_symlink(target_ / "hier"));
    fs::remove(target_ / "hier");
    fs::create_symlink("echt", target_ / "dort");

    run(si, second, &state);

    EXPECT_TRUE(second.report.successful());
    EXPECT_FALSE(fs::is_symlink(source_ / "hier")) << "deleted on the server, so it goes here";
    EXPECT_EQ(fs::read_symlink(source_ / "dort"), fs::path("echt")) << "new on the server, so it comes here";
}

TEST_F(SymlinkSshTest, RealPathOverSftpResolvesALink)  // [SymlinkSshTests.md#RealPathOverSftpResolvesALink]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_directory_symlink("echt", source_ / "ordner");

    const std::optional<std::wstring> resolved = ssh_real_path(remote(source_ / "ordner"));

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(fs::path(*resolved), fs::canonical(source_ / "echt"));
    EXPECT_FALSE(ssh_real_path(remote(source_ / "fehlt" / "tiefer")).has_value());
}

TEST_F(SymlinkSshTest, FollowModeEntersTheDirectoryLinksOfAnSftpSource)  // [SymlinkSshTests.md#FollowModeEntersTheDirectoryLinksOfAnSftpSource]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_directories(root_ / "fremd");
    fs::permissions(root_ / "fremd", fs::perms::all);
    tu::write_text_file(root_ / "fremd" / "f.txt", "f");
    fs::permissions(root_ / "fremd" / "f.txt", fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_symlink("b.txt", source_ / "datei.txt");
    fs::create_directory_symlink(".", source_ / "selbst");
    fs::create_directory_symlink(source_, root_ / "fremd" / "zurueck");
    fs::create_symlink("weg.txt", source_ / "tot.txt");
    tu::write_text_file(target_ / "tot.txt", "liegt schon da");
    SyncItem si = from_remote();
    si.symlink_mode(SymlinkMode::Follow);
    si.delete_in_target(true);
    Outcome first, second;

    run(si, first);

    EXPECT_TRUE(first.report.successful());
    EXPECT_EQ(first.copies, 4u) << "two files of the tree, one behind the directory link and the file link";
    EXPECT_EQ(tu::read_text_file(target_ / "raus" / "f.txt"), "f");
    EXPECT_FALSE(fs::is_symlink(target_ / "raus"));
    EXPECT_EQ(tu::read_text_file(target_ / "datei.txt"), "b") << "over SSH a file link is content in this mode";
    EXPECT_FALSE(fs::exists(target_ / "selbst")) << "a loop on the server";
    EXPECT_FALSE(fs::exists(target_ / "raus" / "zurueck")) << "a loop that closes behind the followed link";
    EXPECT_EQ(tu::read_text_file(target_ / "tot.txt"), "liegt schon da") << "a dead link on the server is no deleted file";

    run(si, second);

    EXPECT_EQ(second.copies, 0u);
}

TEST_F(SymlinkSshTest, FileIsNotWrittenThroughALinkOnAnSftpTarget)  // [SymlinkSshTests.md#FileIsNotWrittenThroughALinkOnAnSftpTarget]
{
    SKIP_WITHOUT_SSH_SERVER();
    tu::write_text_file(root_ / "anderswo.txt", "fremd");
    fs::permissions(root_ / "anderswo.txt", fs::perms::all);
    fs::create_symlink(root_ / "anderswo.txt", target_ / "b.txt");
    fs::create_directories(root_ / "anderswo");
    fs::permissions(root_ / "anderswo", fs::perms::all);
    fs::create_directory_symlink(root_ / "anderswo", target_ / "echt");
    Outcome kept, replaced;

    run(to_remote(), kept);

    EXPECT_EQ(kept.report.errors().size(), 2u) << "one for the file and one for the directory";
    EXPECT_EQ(tu::read_text_file(root_ / "anderswo.txt"), "fremd") << "the upload must not land where the link points";
    EXPECT_TRUE(fs::is_empty(root_ / "anderswo"));
    SyncItem si = to_remote();
    si.delete_in_target(true);

    run(si, replaced);

    EXPECT_TRUE(replaced.report.successful());
    EXPECT_FALSE(fs::is_symlink(target_ / "b.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b");
    EXPECT_FALSE(fs::is_symlink(target_ / "echt"));
    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "a.txt"), "a");
    EXPECT_EQ(tu::read_text_file(root_ / "anderswo.txt"), "fremd");
}

TEST_F(SymlinkSshTest, WildcardSparesADirectoryLinkOfAnSftpSource)  // [SymlinkSshTests.md#WildcardSparesADirectoryLinkOfAnSftpSource]
{
    SKIP_WITHOUT_SSH_SERVER();
    fs::create_directory_symlink("echt", source_ / "ordner");
    fs::create_symlink("b.txt", source_ / "zeiger.dat");
    fs::create_symlink("b.txt", source_ / "zeiger.txt");
    SyncItem si = from_remote();
    si.wildcard(L"*.txt");
    Outcome outcome;

    run(si, outcome);

    EXPECT_TRUE(fs::is_symlink(target_ / "ordner")) << "the wildcard is for files, as with a local source";
    EXPECT_TRUE(fs::is_symlink(target_ / "zeiger.txt"));
    EXPECT_FALSE(fs::exists(fs::symlink_status(target_ / "zeiger.dat"))) << "a link to a file is filtered like a file";
}
#endif
