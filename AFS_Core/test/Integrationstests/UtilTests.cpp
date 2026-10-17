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
#include <Util.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class UtilTest : public ::testing::Test {
protected:
    fs::path root_;

    void SetUp() override
    {
        root_ = fs::temp_directory_path() / "util_test";
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

constexpr size_t COMPARE_BLOCK = 4096;  // [UtilTests.md#COMPARE_BLOCK]

TEST(UtilTests, ToUint16AcceptsTheWholeRange)
{
    EXPECT_EQ(util::to_uint16_t(L"0"), 0);
    EXPECT_EQ(util::to_uint16_t(L"22"), 22);
    EXPECT_EQ(util::to_uint16_t(L"65535"), 65535);
}

TEST(UtilTests, ToUint16RejectsValuesOutsideTheRange)  // [UtilTests.md#ToUint16RejectsValuesOutsideTheRange]
{
    EXPECT_THROW(util::to_uint16_t(L"-1"), std::out_of_range);
    EXPECT_THROW(util::to_uint16_t(L"65536"), std::out_of_range);
    EXPECT_THROW(util::to_uint16_t(L"99999999999999999999"), std::out_of_range);
}

TEST(UtilTests, ToUint16RejectsText)
{
    EXPECT_THROW(util::to_uint16_t(L""), std::invalid_argument);
    EXPECT_THROW(util::to_uint16_t(L"port"), std::invalid_argument);
}

TEST(UtilTests, ToStringOfBoolAndNumbers)
{
    EXPECT_EQ(util::to_string(true), L"true");
    EXPECT_EQ(util::to_string(false), L"false");
    EXPECT_EQ(util::to_string(uint16_t{65535}), L"65535");
    EXPECT_EQ(util::to_string(-7), L"-7");
}

TEST(UtilTests, FileTimeRoundTripsThroughTimeT)  // [UtilTests.md#FileTimeRoundTripsThroughTimeT]
{
    constexpr std::time_t MOMENT = 1700000000;

    EXPECT_EQ(util::to_time_t(util::to_file_time(MOMENT)), MOMENT);
    EXPECT_EQ(util::to_time_t(util::to_file_time(0)), 0);
}

TEST(UtilTests, FileTimeIsPrintedAsUtc)
{
    EXPECT_EQ(util::to_string(util::to_file_time(0)), "Thursday, 01 January 1970 00:00");
}

TEST(UtilTests, EqualAndGreaterCompareFileTimes)
{
    const fs::file_time_type earlier = util::to_file_time(1000);
    const fs::file_time_type later = util::to_file_time(1001);

    EXPECT_TRUE(util::equal(earlier, earlier));
    EXPECT_FALSE(util::equal(earlier, later));
    EXPECT_TRUE(util::greater(later, earlier));
    EXPECT_FALSE(util::greater(earlier, later));
    EXPECT_FALSE(util::greater(earlier, earlier));
}

TEST(UtilTests, TimestampPrefixKeepsTheDirectory)
{
    const fs::path prefixed = util::with_timestamp_prefix(fs::path("dir") / "sub" / "data.txt");

    EXPECT_EQ(prefixed.parent_path(), fs::path("dir") / "sub");
    EXPECT_TRUE(std::regex_match(prefixed.filename().wstring(),
        std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_data\.txt)")));
}

TEST(UtilTests, TimestampPrefixCarriesTheMilliseconds)  // [UtilTests.md#TimestampPrefixCarriesTheMilliseconds]
{
    using namespace std::chrono;
    const system_clock::time_point time = system_clock::from_time_t(1700000000) + milliseconds(7);

    const std::wstring name = util::with_timestamp_prefix("data.txt", time).wstring();

    EXPECT_TRUE(name.ends_with(L"-007_data.txt")) << util::wstring_to_string(name);
}

TEST(UtilTests, TimestampPrefixNeedsAFileName)  // [UtilTests.md#TimestampPrefixNeedsAFileName]
{
    const fs::path directory = fs::path("dir") / "";

    EXPECT_EQ(util::with_timestamp_prefix(directory), directory);
    EXPECT_EQ(util::with_timestamp_prefix(fs::path()), fs::path());
}

TEST(UtilTests, WildmatchIsCaseSensitiveAndAnchored)
{
    EXPECT_FALSE(util::wildmatch(L"A.TXT", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"a.txt.bak", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"file12.txt", L"file?.txt"));
    EXPECT_TRUE(util::wildmatch(L"", L"*"));
    EXPECT_FALSE(util::wildmatch(L"x", L""));
}

TEST_F(UtilTest, FilesEqualComparesContent)
{
    const fs::path original = create_file("a.bin", "content");
    const fs::path copy = create_file("b.bin", "content");
    const fs::path changed = create_file("c.bin", "c0ntent");

    EXPECT_TRUE(util::files_equal(original.wstring(), copy.wstring()));
    EXPECT_FALSE(util::files_equal(original.wstring(), changed.wstring()));
}

TEST_F(UtilTest, FilesEqualSeesADifferenceBehindTheFirstBlock)  // [UtilTests.md#COMPARE_BLOCK]
{
    const std::string content(3 * COMPARE_BLOCK, 'x');
    const fs::path original = create_file("a.bin", content);
    const fs::path copy = create_file("b.bin", content);
    const fs::path changed = create_file("c.bin", content.substr(0, content.size() - 1) + 'y');
    const fs::path longer = create_file("d.bin", content + 'x');

    EXPECT_TRUE(util::files_equal(original.wstring(), copy.wstring()));
    EXPECT_FALSE(util::files_equal(original.wstring(), changed.wstring()));
    EXPECT_FALSE(util::files_equal(original.wstring(), longer.wstring()));
    EXPECT_FALSE(util::files_equal(longer.wstring(), original.wstring()));
}

TEST_F(UtilTest, FilesEqualTreatsEmptyFilesAsEqual)
{
    const fs::path first = create_file("a.bin", "");
    const fs::path second = create_file("b.bin", "");

    EXPECT_TRUE(util::files_equal(first.wstring(), second.wstring()));
}

TEST_F(UtilTest, FilesEqualFailsForAMissingFile)
{
    const fs::path existing = create_file("a.bin", "content");
    const std::wstring missing = (root_ / "missing.bin").wstring();

    EXPECT_FALSE(util::files_equal(existing.wstring(), missing));
    EXPECT_FALSE(util::files_equal(missing, existing.wstring()));
    EXPECT_FALSE(util::files_equal(missing, missing));
}

TEST_F(UtilTest, RenameFileMovesTheContent)
{
    const fs::path old_name = create_file("old.txt", "content");
    const fs::path new_name = root_ / "new.txt";

    EXPECT_TRUE(util::rename_file(old_name.wstring(), new_name.wstring()));

    EXPECT_FALSE(fs::exists(old_name));
    EXPECT_EQ(fs::file_size(new_name), 7u);
}

TEST_F(UtilTest, RenameFileFailsForAMissingSource)
{
    const fs::path new_name = root_ / "new.txt";

    EXPECT_FALSE(util::rename_file((root_ / "missing.txt").wstring(), new_name.wstring()));

    EXPECT_FALSE(fs::exists(new_name));
}

TEST_F(UtilTest, RemoveFileDeletesAnExistingFile)
{
    const fs::path pathname = create_file("data.txt", "content");

    EXPECT_TRUE(util::remove_file(pathname.wstring()));

    EXPECT_FALSE(fs::exists(pathname));
}

TEST_F(UtilTest, RemoveFileFailsForANonEmptyDirectory)  // [UtilTests.md#RemoveFileFailsForANonEmptyDirectory]
{
    create_file("data.txt", "content");

    EXPECT_FALSE(util::remove_file(root_.wstring()));

    EXPECT_TRUE(fs::exists(root_ / "data.txt"));
}

TEST_F(UtilTest, SetLastWriteTimeIsReadBackUnchanged)
{
    const fs::path pathname = create_file("data.txt", "content");
    const fs::file_time_type modify_time = util::to_file_time(1700000000);

    EXPECT_TRUE(util::set_last_write_time(pathname.wstring(), modify_time));

    EXPECT_TRUE(util::equal(fs::last_write_time(pathname), modify_time));
}

TEST_F(UtilTest, SetLastWriteTimeFailsForAMissingFile)
{
    EXPECT_FALSE(util::set_last_write_time((root_ / "missing.txt").wstring(), util::to_file_time(0)));
}

TEST_F(UtilTest, DriveExistsFollowsTheDirectory)
{
    EXPECT_TRUE(util::drive_exists(root_.wstring()));
    EXPECT_FALSE(util::drive_exists((root_ / "missing").wstring()));
}

} // namespace

TEST(UtilTests, WriteUtf8FileWritesTheBytesAndThrowsWhereItCannotWrite)  // [UtilTests.md#WriteUtf8FileWritesTheBytesAndThrowsWhereItCannotWrite]
{
    const fs::path file = fs::temp_directory_path() / ("afx_utf8_" + std::to_string(util::current_pid()) + ".txt");

    util::write_utf8_file(file, L"Grüße");

    std::ifstream in(file, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(bytes, "Gr\xC3\xBC\xC3\x9F" "e");
    fs::remove(file);
    EXPECT_THROW(util::write_utf8_file(file / "unter" / "x.txt", L"x"), std::exception);
}

#ifdef _UNIX
TEST(UtilTests, VolumeExistsLooksForTheVolumeInTheMountTable)  // [UtilTests.md#VolumeExistsLooksForTheVolumeInTheMountTable]
{
    EXPECT_TRUE(util::volume_exists(L"", L"/proc"));
    EXPECT_FALSE(util::volume_exists(L"", L"afx-no-such-volume"));
}
#endif
