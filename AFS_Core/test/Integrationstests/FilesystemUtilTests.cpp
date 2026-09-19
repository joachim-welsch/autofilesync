/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include <FilesystemUtil.h>
#include <Util.h>
#include <optional>
#include <ScopedEnv.h>
#include <filesystem>
#include <fstream>
#include <set>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class FilesystemUtilTest : public ::testing::Test {
protected:
    fs::path root_;

    void SetUp() override
    {
        root_ = fs::temp_directory_path() / "filesystemutil_test";
        fs::remove_all(root_);
        fs::create_directories(root_);
    }

    void TearDown() override
    {
        fs::remove_all(root_);
    }

    fs::path create_file(const std::string& name, const std::string& content) const
    {
        const fs::path pathname = root_ / name;
        std::ofstream(pathname, std::ios::binary) << content;
        return pathname;
    }
};

TEST_F(FilesystemUtilTest, FileExistsSeesFilesAndDirectories)
{
    const fs::path pathname = create_file("data.txt", "content");

    EXPECT_TRUE(fsu::file_exists(pathname.wstring()));
    EXPECT_TRUE(fsu::file_exists(root_.wstring()));
    EXPECT_FALSE(fsu::file_exists((root_ / "missing.txt").wstring()));
    EXPECT_FALSE(fsu::file_exists(L""));
}

TEST_F(FilesystemUtilTest, FileinfoReportsSizeAndWriteTime)
{
    const fs::path pathname = create_file("data.txt", "content");
    const fs::file_time_type modify_time = util::to_file_time(1700000000);
    fs::last_write_time(pathname, modify_time);
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;

    EXPECT_EQ(fsu::fileinfo(pathname.wstring(), filesize, last_write_time), fsu::FileInfoResult::OK);

    EXPECT_EQ(filesize, 7u);
    EXPECT_TRUE(util::equal(last_write_time, modify_time));
}

TEST_F(FilesystemUtilTest, FileinfoHandlesUmlautsInTheName)
{
    const fs::path pathname = root_ / L"Prüfung äöü.txt";
    std::ofstream(pathname) << "abc";
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;

    EXPECT_EQ(fsu::fileinfo(pathname.wstring(), filesize, last_write_time), fsu::FileInfoResult::OK);

    EXPECT_EQ(filesize, 3u);
}

TEST_F(FilesystemUtilTest, FileinfoLeavesTheOutputUntouchedForAMissingFile)  // [FilesystemUtilTests.md#FileinfoLeavesTheOutputUntouchedForAMissingFile]
{
    uint64_t filesize = 42;
    fs::file_time_type last_write_time;

    EXPECT_EQ(fsu::fileinfo((root_ / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::FileNotExist);

    EXPECT_EQ(filesize, 42u);
}

TEST(FilesystemUtilTests, EveryFileInfoResultHasItsOwnMessage)  // [FilesystemUtilTests.md#EveryFileInfoResultHasItsOwnMessage]
{
    using fsu::FileInfoResult;
    std::set<std::wstring> messages;

    for (const FileInfoResult result : { FileInfoResult::FileNotExist, FileInfoResult::DriveNotMounted,
            FileInfoResult::CannotReadFileAttributes, FileInfoResult::InvalidFileTime, FileInfoResult::ErrorReadFileInfo })
        messages.insert(fsu::make_error_message(result));

    EXPECT_EQ(messages.size(), 5u);
    EXPECT_FALSE(messages.contains(L""));
}

TEST_F(FilesystemUtilTest, TouchMovesTheWriteTimeToNow)
{
    const fs::path pathname = create_file("data.txt", "content");
    fs::last_write_time(pathname, util::to_file_time(1000));
    const fs::file_time_type before = fs::file_time_type::clock::now() - std::chrono::seconds(2);  // [FilesystemUtilTests.md#TouchMovesTheWriteTimeToNow]

    EXPECT_TRUE(fsu::touch(pathname.wstring()));

    EXPECT_TRUE(util::greater(fs::last_write_time(pathname), before));
}

TEST_F(FilesystemUtilTest, TouchDoesNotCreateAMissingFile)
{
    const fs::path missing = root_ / "missing.txt";

    EXPECT_FALSE(fsu::touch(missing.wstring()));

    EXPECT_FALSE(fs::exists(missing));
}

TEST(FilesystemUtilTests, HomeDirFollowsTheEnvironment)
{
#ifdef _WIN32
    ScopedEnv drive("HOMEDRIVE", "C:");
    ScopedEnv path("HOMEPATH", R"(\afx_fake_home)");

    EXPECT_EQ(fsu::home_dir(), R"(C:\afx_fake_home)");
#else
    ScopedEnv home("HOME", "/tmp/afx_fake_home");

    EXPECT_EQ(fsu::home_dir(), "/tmp/afx_fake_home");
#endif
}

#ifndef _WIN32
TEST(FilesystemUtilTests, HomeDirFallsBackToThePasswordDatabase)  // [FilesystemUtilTests.md#HomeDirFallsBackToThePasswordDatabase]
{
    ScopedEnv home("HOME", nullptr);

    EXPECT_FALSE(fsu::home_dir().empty());
}

TEST_F(FilesystemUtilTest, MountRootOfAMissingPathIsThatOfItsExistingParent)  // [FilesystemUtilTests.md#MountRootOfAMissingPathIsThatOfItsExistingParent]
{
    const auto existing = fsu::get_mount_root(root_);
    const auto missing = fsu::get_mount_root(root_ / "not" / "yet" / "there");

    ASSERT_TRUE(existing);
    ASSERT_TRUE(missing);
    EXPECT_EQ(*missing, *existing);
    EXPECT_TRUE(fs::exists(*existing));
}

TEST(FilesystemUtilTests, MountRootOfTheRootIsTheRoot)
{
    EXPECT_EQ(fsu::get_mount_root("/"), fs::path("/"));
}
#endif

#ifdef __linux__
TEST(FilesystemUtilTests, ProcIsAMountPointOfItsOwn)  // [FilesystemUtilTests.md#ProcIsAMountPointOfItsOwn]
{
    EXPECT_EQ(fsu::get_mount_root("/proc/self"), fs::path("/proc"));
    EXPECT_EQ(fsu::is_mountpoint("/proc/self"), fs::path("/proc"));
    EXPECT_EQ(fsu::is_mountpoint("/proc"), fs::path("/proc"));
}

TEST(FilesystemUtilTests, TheRootIsNeverReportedAsMountPoint)
{
    EXPECT_FALSE(fsu::is_mountpoint("/"));
}

TEST(FilesystemUtilTests, ASiblingWithTheSamePrefixIsNotCovered)  // [FilesystemUtilTests.md#ASiblingWithTheSamePrefixIsNotCovered]
{
    EXPECT_NE(fsu::is_mountpoint("/procfoo"), fs::path("/proc"));
}
#endif

} // namespace
