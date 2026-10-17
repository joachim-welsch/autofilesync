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
#include "FileUtil.h"
#include "ScopedLog.h"
#include "SshTest.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include "Util.h"
#include <filesystem>
#include <fstream>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

class SymlinkTest : public ::testing::Test {  // [SymlinkTests.md#SymlinkTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("afs_symlinks_" + std::to_string(TestPaths::process_id()));
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";
    const tu::ScopedLog log_{fs::temp_directory_path() / ("autofilesync_symlinks_" + std::to_string(TestPaths::process_id()) + ".log"), LogLevel::Info};

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
        fs::create_directory_symlink(source_ / "echt", source_ / "dirlink");
        fs::create_symlink(source_ / "b.txt", source_ / "filelink.txt");
        Synchronize::start();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem item(SyncItemType type, const std::wstring& source) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source);
        si.target(target_.wstring());
        return si;
    }

    int skipped_links() const  // [SymlinkTests.md#skipped_links]
    {
        std::ifstream in(log_.path());
        const std::string message = translation::translate_utf8("symlink_not_followed");
        int count = 0;
        for (std::string line; std::getline(in, line);)
            if (line.find(message) != std::string::npos)
                ++count;
        return count;
    }
};

} // namespace

TEST_F(SymlinkTest, TreeSyncSkipsADirectorySymlinkAndSaysSo)  // [SymlinkTests.md#TreeSyncSkipsADirectorySymlinkAndSaysSo]
{
    EXPECT_EQ(tu::run_sync(item(SyncItemType::Tree, source_.wstring())), 3u);

    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "a.txt"), "a");
    EXPECT_FALSE(fs::exists(target_ / "dirlink")) << "ein Verzeichnis-Symlink darf weder verfolgt noch als Link angelegt werden";
    EXPECT_EQ(skipped_links(), 1);
}

TEST_F(SymlinkTest, TreeSyncCopiesAFileSymlinkAsItsContent)  // [SymlinkTests.md#file-symlinks-are-content]
{
    tu::run_sync(item(SyncItemType::Tree, source_.wstring()));

    ASSERT_TRUE(fs::exists(target_ / "filelink.txt"));
    EXPECT_FALSE(fs::is_symlink(target_ / "filelink.txt")) << "das Ziel bekommt eine Datei, keinen Link";
    EXPECT_EQ(tu::read_text_file(target_ / "filelink.txt"), "b");
}

TEST_F(SymlinkTest, DirSyncCopiesAFileSymlinkAndIgnoresADirectorySymlink)  // [SymlinkTests.md#file-symlinks-are-content]
{
    EXPECT_EQ(tu::run_sync(item(SyncItemType::Dir, source_.wstring())), 2u);

    EXPECT_EQ(tu::read_text_file(target_ / "filelink.txt"), "b");
    EXPECT_FALSE(fs::is_symlink(target_ / "filelink.txt"));
    EXPECT_FALSE(fs::exists(target_ / "dirlink"));
    EXPECT_FALSE(fs::exists(target_ / "echt")) << "ein Verzeichnis-Item steigt nicht ab";
}

TEST_F(SymlinkTest, TreeSyncFromSshSkipsEverySymlink)  // [SymlinkTests.md#TreeSyncFromSshSkipsEverySymlink]
{
    SKIP_WITHOUT_SSH_SERVER();
    SshAccount account;
    account.set_name("localhost");
    account.set_user(SSHTest::SSH_USER);
    account.set_host(SSHTest::SSH_HOST);
    account.set_password(SSHTest::SSH_PASSWORD);
    Config::instance().add(account);

    EXPECT_EQ(tu::run_sync(item(SyncItemType::Tree, L"localhost:" + source_.wstring())), 2u);

    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "a.txt"), "a");
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b");
    EXPECT_FALSE(fs::exists(target_ / "filelink.txt")) << "ueber SSH wird auch ein Datei-Symlink uebersprungen";
    EXPECT_FALSE(fs::exists(target_ / "dirlink"));
    EXPECT_EQ(skipped_links(), 2);
}
#endif
