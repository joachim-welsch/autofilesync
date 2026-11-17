/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <FilesystemUtil.h>
#include <StrUtil.h>
#include <FileTime.h>
#include <SystemInfo.h>
#include <chrono>
#include <stdexcept>
#include <FileTime.h>
#include <chrono>
#include <string_view>
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

    static std::string tu_content(const fs::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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
    const fs::file_time_type modify_time = file_time::to_file_time(1700000000);
    fs::last_write_time(pathname, modify_time);
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;

    EXPECT_EQ(fsu::fileinfo(pathname.wstring(), filesize, last_write_time), fsu::FileInfoResult::OK);

    EXPECT_EQ(filesize, 7u);
    EXPECT_TRUE(file_time::equal(last_write_time, modify_time));
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

TEST_F(FilesystemUtilTest, FileinfoTellsAMissingDirectoryFromAMissingFile)  // [FilesystemUtilTests.md#FileinfoTellsAMissingDirectoryFromAMissingFile]
{
    uint64_t filesize = 42;
    fs::file_time_type last_write_time;
    create_file("plain.txt", "content");

    EXPECT_EQ(fsu::fileinfo((root_ / "gone" / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::DirectoryNotExist);
    EXPECT_EQ(fsu::fileinfo((root_ / "gone" / "deeper" / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::DirectoryNotExist);
    EXPECT_EQ(fsu::fileinfo((root_ / "plain.txt" / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::DirectoryNotExist)
        << "a file where the directory should be is no directory either";
    EXPECT_EQ(fsu::fileinfo((root_ / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::FileNotExist);
    EXPECT_EQ(filesize, 42u);
}

TEST_F(FilesystemUtilTest, FileinfoOfADirectoryThatExistsIsNotAMissingDirectory)
{
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;
    fs::create_directories(root_ / "there");

    EXPECT_EQ(fsu::fileinfo((root_ / "there" / "missing.txt").wstring(), filesize, last_write_time), fsu::FileInfoResult::FileNotExist);
}

#ifndef _WIN32
TEST_F(FilesystemUtilTest, FileinfoTellsARefusedFileFromAMissingOne)  // [FilesystemUtilTests.md#FileinfoTellsARefusedFileFromAMissingOne]
{
    if (geteuid() == 0)
        GTEST_SKIP() << "root is refused nothing";
    uint64_t filesize = 42;
    fs::file_time_type last_write_time;
    fs::create_directories(root_ / "locked" / "deeper");
    std::ofstream(root_ / "locked" / "there.txt") << "content";
    fs::permissions(root_ / "locked", fs::perms::none);

    const fsu::FileInfoResult there = fsu::fileinfo((root_ / "locked" / "there.txt").wstring(), filesize, last_write_time);
    const fsu::FileInfoResult missing = fsu::fileinfo((root_ / "locked" / "missing.txt").wstring(), filesize, last_write_time);
    const fsu::FileInfoResult deeper = fsu::fileinfo((root_ / "locked" / "deeper" / "missing.txt").wstring(), filesize, last_write_time);
    fs::permissions(root_ / "locked", fs::perms::owner_all);

    EXPECT_EQ(there, fsu::FileInfoResult::PermissionDenied);
    EXPECT_EQ(missing, fsu::FileInfoResult::PermissionDenied) << "behind a locked directory nothing is known to be missing";
    EXPECT_EQ(deeper, fsu::FileInfoResult::PermissionDenied);
    EXPECT_EQ(filesize, 42u);
}

TEST_F(FilesystemUtilTest, FileinfoOfAFileThatMayNotBeReadStillAnswers)  // [FilesystemUtilTests.md#FileinfoOfAFileThatMayNotBeReadStillAnswers]
{
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;
    const fs::path pathname = create_file("secret.txt", "content");
    fs::permissions(pathname, fs::perms::none);

    EXPECT_EQ(fsu::fileinfo(pathname.wstring(), filesize, last_write_time), fsu::FileInfoResult::OK);
    EXPECT_EQ(filesize, 7u);
}
#endif

TEST(FilesystemUtilTests, ReasonForFailureTellsLockedFromRefusedFromMissing)  // [FilesystemUtilTests.md#ReasonForFailureTellsLockedFromRefusedFromMissing]
{
    using fsu::FileInfoResult;
    EXPECT_EQ(fsu::reason_for_failure(std::make_error_code(std::errc::permission_denied)), FileInfoResult::PermissionDenied);
    EXPECT_EQ(fsu::reason_for_failure(std::make_error_code(std::errc::no_such_file_or_directory)), FileInfoResult::FileNotExist);
    EXPECT_EQ(fsu::reason_for_failure(std::make_error_code(std::errc::io_error)), FileInfoResult::ErrorReadFileInfo);
#ifdef _WIN32
    EXPECT_EQ(fsu::reason_for_failure(std::error_code(32, std::system_category())), FileInfoResult::FileLocked) << "ERROR_SHARING_VIOLATION";
    EXPECT_EQ(fsu::reason_for_failure(std::error_code(33, std::system_category())), FileInfoResult::FileLocked) << "ERROR_LOCK_VIOLATION";
#else
    EXPECT_EQ(fsu::reason_for_failure(std::make_error_code(std::errc::text_file_busy)), FileInfoResult::FileLocked);
    EXPECT_EQ(fsu::reason_for_failure(std::make_error_code(std::errc::device_or_resource_busy)), FileInfoResult::FileLocked);
#endif
}

TEST_F(FilesystemUtilTest, CompareContentTellsDifferentFromNotReadable)  // [FilesystemUtilTests.md#CompareContentTellsDifferentFromNotReadable]
{
    const fs::path original = create_file("a.bin", "content");
    const fs::path copy = create_file("b.bin", "content");
    const fs::path changed = create_file("c.bin", "c0ntent");
    const std::wstring missing = (root_ / "missing.bin").wstring();

    const fsu::ContentComparison same = fsu::compare_content(original.wstring(), copy.wstring());
    const fsu::ContentComparison different = fsu::compare_content(original.wstring(), changed.wstring());
    const fsu::ContentComparison second_missing = fsu::compare_content(original.wstring(), missing);
    const fsu::ContentComparison first_missing = fsu::compare_content(missing, original.wstring());

    EXPECT_TRUE(same.equal);
    EXPECT_EQ(same.refusal, fsu::FileInfoResult::OK);
    EXPECT_FALSE(different.equal);
    EXPECT_EQ(different.refusal, fsu::FileInfoResult::OK) << "two files that differ were both read";
    EXPECT_FALSE(second_missing.equal);
    EXPECT_EQ(second_missing.refusal, fsu::FileInfoResult::FileNotExist);
    EXPECT_EQ(second_missing.refused_file, missing);
    EXPECT_EQ(first_missing.refusal, fsu::FileInfoResult::FileNotExist);
    EXPECT_EQ(first_missing.refused_file, missing);
}

#ifndef _WIN32
TEST_F(FilesystemUtilTest, FileThatDoesNotOpenIsRefusedNotDifferent)  // [FilesystemUtilTests.md#FileThatDoesNotOpenIsRefusedNotDifferent]
{
    if (geteuid() == 0)
        GTEST_SKIP() << "root is refused nothing";
    const fs::path open = create_file("a.bin", "content");
    const fs::path closed = create_file("b.bin", "content");
    fs::permissions(closed, fs::perms::none);

    const fsu::ContentComparison first = fsu::compare_content(closed.wstring(), open.wstring());
    const fsu::ContentComparison second = fsu::compare_content(open.wstring(), closed.wstring());

    EXPECT_EQ(fsu::read_refusal(open.wstring()), fsu::FileInfoResult::OK);
    EXPECT_EQ(fsu::read_refusal(closed.wstring()), fsu::FileInfoResult::PermissionDenied);
    EXPECT_EQ(fsu::read_refusal((root_ / "missing.bin").wstring()), fsu::FileInfoResult::FileNotExist);
    EXPECT_EQ(first.refusal, fsu::FileInfoResult::PermissionDenied);
    EXPECT_EQ(first.refused_file, closed.wstring());
    EXPECT_EQ(second.refusal, fsu::FileInfoResult::PermissionDenied);
    EXPECT_EQ(second.refused_file, closed.wstring());
    EXPECT_FALSE(first.equal || second.equal);
    EXPECT_FALSE(fsu::files_equal(open.wstring(), closed.wstring())) << "files_equal() keeps its answer for callers that only verify a copy";
}
#endif

TEST_F(FilesystemUtilTest, CreateDirectoryRefusalSaysWhyADirectoryIsNotThere)  // [FilesystemUtilTests.md#CreateDirectoryRefusalSaysWhyADirectoryIsNotThere]
{
    std::error_code ec;
    create_file("plain.txt", "content");

    EXPECT_EQ(fsu::create_directory_refusal(root_ / "new" / "deeper", ec), fsu::FileInfoResult::OK);
    EXPECT_TRUE(fs::is_directory(root_ / "new" / "deeper"));
    EXPECT_EQ(fsu::create_directory_refusal(root_ / "new", ec), fsu::FileInfoResult::OK) << "a directory that is there needs no creating";
    EXPECT_FALSE(ec);
    EXPECT_EQ(fsu::create_directory_refusal(root_ / "plain.txt", ec), fsu::FileInfoResult::CannotCreateDirectory) << "a file bears the name";
    EXPECT_TRUE(ec);
    EXPECT_EQ(fsu::create_directory_refusal(root_ / "plain.txt" / "below", ec), fsu::FileInfoResult::CannotCreateDirectory);
    EXPECT_EQ(tu_content(root_ / "plain.txt"), "content");
}

#ifndef _WIN32
TEST_F(FilesystemUtilTest, CreateDirectoryRefusalTellsARefusalFromOtherFailures)
{
    if (geteuid() == 0)
        GTEST_SKIP() << "root is refused nothing";
    std::error_code ec;
    fs::create_directories(root_ / "locked");
    fs::permissions(root_ / "locked", fs::perms::owner_read | fs::perms::owner_exec);

    const fsu::FileInfoResult refusal = fsu::create_directory_refusal(root_ / "locked" / "new", ec);
    fs::permissions(root_ / "locked", fs::perms::owner_all);

    EXPECT_EQ(refusal, fsu::FileInfoResult::PermissionDenied);
    EXPECT_FALSE(fs::exists(root_ / "locked" / "new"));
}
#endif

TEST(FilesystemUtilTests, FailureTextNamesKnownReasonsAndFallsBackToTheSystem)  // [FilesystemUtilTests.md#FailureTextNamesKnownReasonsAndFallsBackToTheSystem]
{
    using fsu::FileInfoResult;
    const std::error_code no_space = std::make_error_code(std::errc::no_space_on_device);

    EXPECT_EQ(fsu::failure_text(std::make_error_code(std::errc::permission_denied)), fsu::make_error_message(FileInfoResult::PermissionDenied));
    EXPECT_EQ(fsu::failure_text(std::make_error_code(std::errc::no_such_file_or_directory)), fsu::make_error_message(FileInfoResult::FileNotExist));
    EXPECT_EQ(util::wstring_to_string(fsu::failure_text(no_space)), no_space.message()) << "an error without a text of its own keeps the system's";
    EXPECT_EQ(fsu::failure_text(std::error_code()), fsu::make_error_message(FileInfoResult::OK)) << "no error code at all is the unknown error";
}

TEST(FilesystemUtilTests, EveryFileInfoResultHasItsOwnMessage)  // [FilesystemUtilTests.md#EveryFileInfoResultHasItsOwnMessage]
{
    using fsu::FileInfoResult;
    std::set<std::wstring> messages;

    for (const FileInfoResult result : { FileInfoResult::FileNotExist, FileInfoResult::DirectoryNotExist, FileInfoResult::DriveNotMounted,
            FileInfoResult::CannotReadFileAttributes, FileInfoResult::PermissionDenied, FileInfoResult::FileLocked, FileInfoResult::InvalidFileTime, FileInfoResult::ErrorReadFileInfo, FileInfoResult::CannotCreateDirectory })
        messages.insert(fsu::make_error_message(result));

    EXPECT_EQ(messages.size(), 9u);
    EXPECT_FALSE(messages.contains(fsu::make_error_message(FileInfoResult::OK))) << "no failure may share the text of the unknown error";
    EXPECT_FALSE(messages.contains(L""));
}

TEST_F(FilesystemUtilTest, TouchMovesTheWriteTimeToNow)
{
    const fs::path pathname = create_file("data.txt", "content");
    fs::last_write_time(pathname, file_time::to_file_time(1000));
    const fs::file_time_type before = fs::file_time_type::clock::now() - std::chrono::seconds(2);  // [FilesystemUtilTests.md#TouchMovesTheWriteTimeToNow]

    EXPECT_TRUE(fsu::touch(pathname.wstring()));

    EXPECT_TRUE(file_time::greater(fs::last_write_time(pathname), before));
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
    ScopedEnv profile("USERPROFILE", R"(C:\afx_fake_home)");

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

TEST(FilesystemUtilTests, PlainEntryNameRefusesSeparatorsAndDotEntries)  // [FilesystemUtilTests.md#PlainEntryNameRefusesSeparatorsAndDotEntries]
{
    EXPECT_TRUE(fsu::plain_entry_name("photo.jpg"));
    EXPECT_TRUE(fsu::plain_entry_name("..hidden"));
    EXPECT_TRUE(fsu::plain_entry_name("Umlaut \xc3\xa4.txt"));
    EXPECT_FALSE(fsu::plain_entry_name(""));
    EXPECT_FALSE(fsu::plain_entry_name("."));
    EXPECT_FALSE(fsu::plain_entry_name(".."));
    EXPECT_FALSE(fsu::plain_entry_name("../../.ssh/authorized_keys"));
    EXPECT_FALSE(fsu::plain_entry_name("a/b"));
    EXPECT_FALSE(fsu::plain_entry_name("..\\..\\x"));
    EXPECT_FALSE(fsu::plain_entry_name(std::string_view("a\0b", 3)));
}

namespace {
constexpr size_t COMPARE_BLOCK = 4096;  // [FilesystemUtilTests.md#COMPARE_BLOCK]
}

TEST_F(FilesystemUtilTest, FilesEqualComparesContent)
{
    const fs::path original = create_file("a.bin", "content");
    const fs::path copy = create_file("b.bin", "content");
    const fs::path changed = create_file("c.bin", "c0ntent");

    EXPECT_TRUE(fsu::files_equal(original.wstring(), copy.wstring()));
    EXPECT_FALSE(fsu::files_equal(original.wstring(), changed.wstring()));
}

TEST_F(FilesystemUtilTest, FilesEqualSeesADifferenceBehindTheFirstBlock)  // [FilesystemUtilTests.md#COMPARE_BLOCK]
{
    const std::string content(3 * COMPARE_BLOCK, 'x');
    const fs::path original = create_file("a.bin", content);
    const fs::path copy = create_file("b.bin", content);
    const fs::path changed = create_file("c.bin", content.substr(0, content.size() - 1) + 'y');
    const fs::path longer = create_file("d.bin", content + 'x');

    EXPECT_TRUE(fsu::files_equal(original.wstring(), copy.wstring()));
    EXPECT_FALSE(fsu::files_equal(original.wstring(), changed.wstring()));
    EXPECT_FALSE(fsu::files_equal(original.wstring(), longer.wstring()));
    EXPECT_FALSE(fsu::files_equal(longer.wstring(), original.wstring()));
}

TEST_F(FilesystemUtilTest, FilesEqualTreatsEmptyFilesAsEqual)
{
    const fs::path first = create_file("a.bin", "");
    const fs::path second = create_file("b.bin", "");

    EXPECT_TRUE(fsu::files_equal(first.wstring(), second.wstring()));
}

TEST_F(FilesystemUtilTest, FilesEqualFailsForAMissingFile)
{
    const fs::path existing = create_file("a.bin", "content");
    const std::wstring missing = (root_ / "missing.bin").wstring();

    EXPECT_FALSE(fsu::files_equal(existing.wstring(), missing));
    EXPECT_FALSE(fsu::files_equal(missing, existing.wstring()));
    EXPECT_FALSE(fsu::files_equal(missing, missing));
}

TEST_F(FilesystemUtilTest, RenameFileMovesTheContent)
{
    const fs::path old_name = create_file("old.txt", "content");
    const fs::path new_name = root_ / "new.txt";

    EXPECT_TRUE(fsu::rename_file(old_name.wstring(), new_name.wstring()));

    EXPECT_FALSE(fs::exists(old_name));
    EXPECT_EQ(fs::file_size(new_name), 7u);
}

TEST_F(FilesystemUtilTest, RenameFileFailsForAMissingSource)
{
    const fs::path new_name = root_ / "new.txt";

    EXPECT_FALSE(fsu::rename_file((root_ / "missing.txt").wstring(), new_name.wstring()));

    EXPECT_FALSE(fs::exists(new_name));
}

TEST_F(FilesystemUtilTest, RemoveFileDeletesAnExistingFile)
{
    const fs::path pathname = create_file("data.txt", "content");

    EXPECT_TRUE(fsu::remove_file(pathname.wstring()));

    EXPECT_FALSE(fs::exists(pathname));
}

TEST_F(FilesystemUtilTest, RemoveFileFailsForANonEmptyDirectory)  // [FilesystemUtilTests.md#RemoveFileFailsForANonEmptyDirectory]
{
    create_file("data.txt", "content");

    EXPECT_FALSE(fsu::remove_file(root_.wstring()));

    EXPECT_TRUE(fs::exists(root_ / "data.txt"));
}

TEST_F(FilesystemUtilTest, SetLastWriteTimeIsReadBackUnchanged)
{
    const fs::path pathname = create_file("data.txt", "content");
    const fs::file_time_type modify_time = file_time::to_file_time(1700000000);

    EXPECT_TRUE(fsu::set_last_write_time(pathname.wstring(), modify_time));

    EXPECT_TRUE(file_time::equal(fs::last_write_time(pathname), modify_time));
}

TEST_F(FilesystemUtilTest, SetLastWriteTimeFailsForAMissingFile)
{
    EXPECT_FALSE(fsu::set_last_write_time((root_ / "missing.txt").wstring(), file_time::to_file_time(0)));
}

TEST_F(FilesystemUtilTest, DriveExistsFollowsTheDirectory)
{
    EXPECT_TRUE(fsu::drive_exists(root_.wstring()));
    EXPECT_FALSE(fsu::drive_exists((root_ / "missing").wstring()));
}

TEST(FilesystemUtilTests, WriteUtf8FileWritesTheBytesAndThrowsWhereItCannotWrite)  // [FilesystemUtilTests.md#WriteUtf8FileWritesTheBytesAndThrowsWhereItCannotWrite]
{
    const fs::path file = fs::temp_directory_path() / ("afx_utf8_" + std::to_string(system_info::current_pid()) + ".txt");

    fsu::write_utf8_file(file, L"Grüße");

    std::ifstream in(file, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();  // Windows refuses to remove an open file
    EXPECT_EQ(bytes, "Gr\xC3\xBC\xC3\x9F" "e");
    fs::remove(file);
    EXPECT_THROW(fsu::write_utf8_file(file / "unter" / "x.txt", L"x"), std::exception);
}

#ifdef _UNIX
TEST(FilesystemUtilTests, VolumeExistsLooksForTheVolumeInTheMountTable)  // [FilesystemUtilTests.md#VolumeExistsLooksForTheVolumeInTheMountTable]
{
#ifdef _MACOS
    EXPECT_TRUE(fsu::volume_exists(L"", L"/dev"));
#else
    EXPECT_TRUE(fsu::volume_exists(L"", L"/proc"));
#endif
    EXPECT_FALSE(fsu::volume_exists(L"", L"afx-no-such-volume"));
}
#endif

TEST_F(FilesystemUtilTest, WritePrivateFileIsOwnerOnlyFromTheStart)  // [FilesystemUtilTests.md#WritePrivateFileIsOwnerOnlyFromTheStart]
{
    const fs::path secret = root_ / "secret.key";
    const fs::path wide = create_file("wide.key", "old");
    fs::permissions(wide, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);

    ASSERT_TRUE(fsu::write_private_file(secret, std::string_view("k\0y", 3)));
    ASSERT_TRUE(fsu::write_private_file(wide, "new"));

    std::ifstream in(secret, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(bytes, std::string("k\0y", 3));
#ifndef _WIN32
    EXPECT_EQ(fs::status(secret).permissions(), fs::perms::owner_read | fs::perms::owner_write);
    EXPECT_EQ(fs::status(wide).permissions(), fs::perms::owner_read | fs::perms::owner_write);
#endif
    EXPECT_FALSE(fsu::write_private_file(root_ / "missing" / "secret.key", "x"));
}

TEST(FilesystemUtilTests, RemoveFileReturnsWhetherSomethingWasActuallyRemoved)  // [FilesystemUtilTests.md#RemoveFileReturnsWhetherSomethingWasActuallyRemoved]
{
    const std::wstring non_existent = L"/nonexistent_afx_dir/nonexistent_afx_file.txt";
    EXPECT_FALSE(fsu::remove_file(non_existent, LogLevel::None));
}
