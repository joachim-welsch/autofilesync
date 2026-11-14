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
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <filesystem>
#include <fstream>
#include <regex>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class FileDeleteTest : public ::testing::Test {
protected:
    fs::path root_;
    fs::path bin_;
    fs::path previous_bin_;

    void SetUp() override {
        previous_bin_ = RecycleBin::instance().bin_path();
        root_ = fs::temp_directory_path() / "filedelete_test";
        fs::remove_all(root_);
        fs::create_directories(root_ / "sub");
        bin_ = root_ / "bin";
        RecycleBin::instance().bin_path(bin_);
        Config::instance().settings().delete_mode(DeleteMode::RecycleBin);
    }

    void TearDown() override {
        Config::instance().reset();
        RecycleBin::instance().bin_path(previous_bin_);
        fs::remove_all(root_);
    }

    fs::path create_file(const fs::path& relative, const std::string& content = "x") const {
        const fs::path pathname = root_ / relative;
        fs::create_directories(pathname.parent_path());
        std::ofstream os(pathname);
        os << content;
        return pathname;
    }

    static bool has_timestamp_prefix(const fs::path& item, const std::wstring& original_name) {
        return std::regex_match(item.filename().wstring(),
            std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_)" + original_name));
    }
};

TEST_F(FileDeleteTest, RecycleBinModeMovesFileIntoBin)
{
    const fs::path source = create_file("sub/data.txt", "content");

    EXPECT_TRUE(delete_file(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(has_timestamp_prefix(items[0], L"data\\.txt"));
}

TEST_F(FileDeleteTest, RecycleBinModeMirrorsTheOriginDirectory)  // [FileDeleteTests.md#RecycleBinModeMirrorsTheOriginDirectory]
{
    const fs::path source = create_file("sub/data.txt");

    ASSERT_TRUE(delete_file(source.wstring()));

    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].parent_path(), source.parent_path().relative_path());
}

TEST_F(FileDeleteTest, RecycleBinModeKeepsFilesOfTheSameName)
{
    const fs::path first = create_file("sub/data.txt", "first");
    const fs::path second = create_file("other/data.txt", "second");

    ASSERT_TRUE(delete_file(first.wstring()));
    ASSERT_TRUE(delete_file(second.wstring()));

    EXPECT_EQ(RecycleBin::instance().list_items().size(), 2u);
}

TEST_F(FileDeleteTest, RecycleBinModeKeepsEveryDeletionOfTheSameFile)  // [FileDeleteTests.md#RecycleBinModeKeepsEveryDeletionOfTheSameFile]
{
    constexpr std::size_t DELETIONS = 200;
    for (std::size_t i = 0; i < DELETIONS; ++i)
        ASSERT_TRUE(delete_file(create_file("sub/data.txt", std::to_string(i)).wstring()));

    EXPECT_EQ(RecycleBin::instance().list_items().size(), DELETIONS);
}

TEST_F(FileDeleteTest, PermanentModeDeletesWithoutTouchingTheBin)
{
    Config::instance().settings().delete_mode(DeleteMode::Permanent);
    const fs::path source = create_file("sub/data.txt");

    EXPECT_TRUE(delete_file(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(FileDeleteTest, MissingFileFailsInBothModes)
{
    const fs::path missing = root_ / "sub" / "gone.txt";

    EXPECT_FALSE(delete_file(missing.wstring()));
    Config::instance().settings().delete_mode(DeleteMode::Permanent);
    EXPECT_FALSE(delete_file(missing.wstring()));
}

TEST_F(FileDeleteTest, RecycleBinModeMovesDirectories)
{
    create_file("sub/tree/inner.txt");

    EXPECT_TRUE(delete_file((root_ / "sub" / "tree").wstring()));

    EXPECT_FALSE(fs::exists(root_ / "sub" / "tree"));
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].filename(), fs::path(L"inner.txt"));
}

} // namespace
