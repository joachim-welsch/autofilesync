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
#include "FileUtil.h"
#include "SshDirInspector.h"
#include "SshTest.h"
#include "TestPaths.h"
#include <algorithm>
#include <filesystem>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

class SshDirInspectorTest : public ::testing::Test {  // [SshDirInspectorTests.md#SshDirInspectorTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("afs_dirinspector_" + std::to_string(TestPaths::process_id()));

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        fs::remove_all(root_);
        for (const fs::path& dir : {root_, root_ / "unter"}) {
            fs::create_directories(dir);
            fs::permissions(dir, fs::perms::all);
        }
        tu::write_text_file(root_ / "datei.txt", "Inhalt");
        tu::write_text_file(root_ / "unter" / "tief.txt", "tief");
        for (const char* file : {"datei.txt", "unter/tief.txt"})
            fs::permissions(root_ / file, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
        fs::create_symlink(root_ / "datei.txt", root_ / "link.txt");
        fs::create_directory_symlink(root_ / "unter", root_ / "dirlink");
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    static SshAccount account()
    {
        SshAccount account;
        account.set_name("localhost");
        account.set_user(SSHTest::SSH_USER);
        account.set_host(SSHTest::SSH_HOST);
        account.set_password(SSHTest::SSH_PASSWORD);
        return account;
    }

    static const RemoteDirEntry* find(const RemoteDirInfo& info, const std::string& name)
    {
        const auto it = std::find_if(info.entries.begin(), info.entries.end(), [&](const RemoteDirEntry& e) { return e.name == name; });
        return it == info.entries.end() ? nullptr : &*it;
    }
};

} // namespace

TEST_F(SshDirInspectorTest, InspectDirListsEntriesWithTheirTypes)  // [SshDirInspectorTests.md#InspectDirListsEntriesWithTheirTypes]
{
    SshDirInspector inspector(account());

    const RemoteDirInfo info = inspector.inspect_dir(root_.string());

    EXPECT_TRUE(info.exists);
    EXPECT_TRUE(info.is_directory);
    EXPECT_TRUE(info.error_message.empty()) << info.error_message;
    EXPECT_EQ(info.entries.size(), 4u) << ". und .. gehoeren nicht dazu";
    const RemoteDirEntry* file = find(info, "datei.txt");
    ASSERT_NE(file, nullptr);
    EXPECT_TRUE(file->is_regular_file);
    EXPECT_EQ(file->size_bytes, 6u);
    EXPECT_GT(file->raw_mtime, 0);
    EXPECT_EQ(file->last_modified_iso.size(), std::string("2026-10-17 07:00:00").size());
    const RemoteDirEntry* dir = find(info, "unter");
    ASSERT_NE(dir, nullptr);
    EXPECT_TRUE(dir->is_directory);
    const RemoteDirEntry* link = find(info, "link.txt");
    ASSERT_NE(link, nullptr);
    EXPECT_TRUE(link->is_symlink);
    EXPECT_FALSE(link->is_regular_file) << "readdir sieht den Link, nicht sein Ziel";
}

TEST_F(SshDirInspectorTest, InspectDirRecursiveReachesNestedFilesButNotLinkedDirectories)  // [SshDirInspectorTests.md#InspectDirRecursiveReachesNestedFilesButNotLinkedDirectories]
{
    SshDirInspector inspector(account());

    const RemoteDirInfo info = inspector.inspect_dir_recursive(root_.string());

    EXPECT_TRUE(info.error_message.empty()) << info.error_message;
    EXPECT_NE(find(info, (root_ / "unter" / "tief.txt").string()), nullptr) << "verschachtelte Eintraege tragen ihren vollen Pfad";
    EXPECT_NE(find(info, (root_ / "dirlink").string()), nullptr);
    EXPECT_EQ(find(info, (root_ / "dirlink" / "tief.txt").string()), nullptr) << "ein Verzeichnis-Link wird nicht betreten";
}

TEST_F(SshDirInspectorTest, MissingPathAndPlainFileAreReportedNotListed)  // [SshDirInspectorTests.md#MissingPathAndPlainFileAreReportedNotListed]
{
    SshDirInspector inspector(account());

    const RemoteDirInfo missing = inspector.inspect_dir((root_ / "fehlt").string());
    const RemoteDirInfo file = inspector.inspect_dir((root_ / "datei.txt").string());

    EXPECT_FALSE(missing.exists);
    EXPECT_FALSE(missing.error_message.empty());
    EXPECT_TRUE(missing.entries.empty());
    EXPECT_TRUE(file.exists);
    EXPECT_FALSE(file.is_directory);
    EXPECT_FALSE(file.error_message.empty());
    EXPECT_TRUE(file.entries.empty());
}

TEST_F(SshDirInspectorTest, HomeDirIsAnAbsoluteExistingDirectory)  // [SshDirInspectorTests.md#HomeDirIsAnAbsoluteExistingDirectory]
{
    SshDirInspector inspector(account());

    const std::string home = inspector.get_home_dir();

    ASSERT_FALSE(home.empty());
    EXPECT_EQ(home.front(), '/');
    EXPECT_TRUE(fs::is_directory(home)) << home;
}
#endif
