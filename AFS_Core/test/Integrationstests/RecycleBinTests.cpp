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
#include <RecycleBin.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>
#ifdef __linux__
#include <chrono>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class RecycleBinTest : public ::testing::Test {  // [RecycleBinTests.md#RecycleBinTest]
protected:
    fs::path root_;
    fs::path bin_;
    fs::path previous_cwd_;
    fs::path previous_bin_;

    void SetUp() override
    {
        root_ = fs::temp_directory_path() / "recyclebin_test";
        fs::remove_all(root_);
        fs::create_directories(root_ / "work");
        bin_ = root_ / "bin";
        previous_cwd_ = fs::current_path();
        previous_bin_ = RecycleBin::instance().bin_path();
        fs::current_path(root_ / "work");
        RecycleBin::instance().bin_path(bin_);
    }

    void TearDown() override
    {
        RecycleBin::instance().bin_path(previous_bin_);
        fs::current_path(previous_cwd_);
        fs::remove_all(root_);
    }

    static void create_file(const fs::path& pathname, const std::string& content = "content")
    {
        if (pathname.has_parent_path())
            fs::create_directories(pathname.parent_path());
        std::ofstream(pathname, std::ios::binary) << content;
    }

    static std::string content_of(const fs::path& pathname)
    {
        std::ifstream is(pathname, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(is), {});
    }
};

TEST_F(RecycleBinTest, AddItemMirrorsTheRelativePathInsideTheBin)
{
    const fs::path item = fs::path("sub") / "data.txt";
    create_file(item);

    EXPECT_TRUE(RecycleBin::instance().add_item(item));

    EXPECT_FALSE(fs::exists(item));
    EXPECT_EQ(content_of(bin_ / item), "content");
    EXPECT_EQ(RecycleBin::instance().list_items(), RecycleBin::ItemList{item});
}

TEST_F(RecycleBinTest, RestoreItemPutsTheFileBackWhereItCameFrom)
{
    const fs::path item = fs::path("sub") / "data.txt";
    create_file(item);
    ASSERT_TRUE(RecycleBin::instance().add_item(item));
    fs::remove_all("sub");  // [RecycleBinTests.md#RestoreItemPutsTheFileBackWhereItCameFrom]

    EXPECT_TRUE(RecycleBin::instance().restore_item(item));

    EXPECT_EQ(content_of(item), "content");
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinTest, RestoreItemNeverOverwritesAnExistingFile)  // [RecycleBinTests.md#RestoreItemNeverOverwritesAnExistingFile]
{
    const fs::path item = "data.txt";
    create_file(item, "deleted");
    ASSERT_TRUE(RecycleBin::instance().add_item(item));
    create_file(item, "newer");

    EXPECT_FALSE(RecycleBin::instance().restore_item(item));

    EXPECT_EQ(content_of(item), "newer");
    EXPECT_EQ(content_of(bin_ / item), "deleted");
}

TEST_F(RecycleBinTest, RestoreItemFailsForAnItemThatIsNotInTheBin)
{
    EXPECT_FALSE(RecycleBin::instance().restore_item("missing.txt"));

    EXPECT_FALSE(fs::exists("missing.txt"));
}

TEST_F(RecycleBinTest, AddItemFailsForAMissingSource)
{
    EXPECT_FALSE(RecycleBin::instance().add_item("missing.txt"));

    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinTest, AbsoluteAndEmptyItemsAreRejected)  // [RecycleBinTests.md#items-must-stay-inside-the-bin]
{
    const fs::path absolute = fs::current_path() / "data.txt";
    create_file(absolute);

    EXPECT_FALSE(RecycleBin::instance().add_item(absolute));
    EXPECT_FALSE(RecycleBin::instance().add_item(absolute, fs::path()));
    EXPECT_FALSE(RecycleBin::instance().restore_item(absolute));

    EXPECT_TRUE(fs::exists(absolute));
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinTest, ItemsEscapingTheBinAreRejected)  // [RecycleBinTests.md#items-must-stay-inside-the-bin]
{
    const fs::path escaping = fs::path("..") / "outside.txt";
    const fs::path hidden_escape = fs::path("sub") / ".." / ".." / "outside.txt";
    create_file(escaping);

    EXPECT_FALSE(RecycleBin::instance().add_item(escaping));
    EXPECT_FALSE(RecycleBin::instance().add_item(escaping, hidden_escape));
    EXPECT_FALSE(RecycleBin::instance().restore_item(escaping));

    EXPECT_TRUE(fs::exists(escaping));
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinTest, DotDotThatStaysInsideIsNormalized)
{
    const fs::path item = fs::path("sub") / ".." / "data.txt";
    create_file("data.txt");
    fs::create_directory("sub");  // the OS resolves "sub/.." only if sub exists

    EXPECT_TRUE(RecycleBin::instance().add_item(item));

    EXPECT_EQ(RecycleBin::instance().list_items(), RecycleBin::ItemList{"data.txt"});
}

TEST_F(RecycleBinTest, SourceOutsideTheWorkingDirectoryGetsTheGivenName)
{
    const fs::path source = root_ / "elsewhere" / "data.txt";
    const fs::path item = fs::path("elsewhere") / "2024-01-01_data.txt";
    create_file(source);

    EXPECT_TRUE(RecycleBin::instance().add_item(source, item));

    EXPECT_FALSE(fs::exists(source));
    EXPECT_EQ(RecycleBin::instance().list_items(), RecycleBin::ItemList{item});
}

TEST_F(RecycleBinTest, ListItemsOfABinThatDoesNotExistYetIsEmpty)
{
    ASSERT_FALSE(fs::exists(bin_));

    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinTest, ListItemsSurvivesANewBinPath)  // [RecycleBinTests.md#ListItemsSurvivesANewBinPath]
{
    create_file("data.txt");
    ASSERT_TRUE(RecycleBin::instance().add_item("data.txt"));

    RecycleBin::instance().bin_path(root_ / "other_bin");
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
    RecycleBin::instance().bin_path(bin_);

    EXPECT_EQ(RecycleBin::instance().list_items().size(), 1u);
}

#ifndef _WIN32
TEST_F(RecycleBinTest, ADanglingSymlinkCanBeBinnedAndRestored)  // [RecycleBinTests.md#ADanglingSymlinkCanBeBinnedAndRestored]
{
    const fs::path item = "dangling";
    fs::create_symlink("no_such_target", item);

    EXPECT_TRUE(RecycleBin::instance().add_item(item));
    EXPECT_FALSE(fs::is_symlink(item));
    EXPECT_TRUE(RecycleBin::instance().restore_item(item));

    EXPECT_EQ(fs::read_symlink(item), fs::path("no_such_target"));
}
#endif

TEST_F(RecycleBinTest, ConcurrentAddsLoseNoItem)  // [RecycleBinTests.md#ConcurrentAddsLoseNoItem]
{
    constexpr int THREADS = 8;
    constexpr int ITEMS_PER_THREAD = 25;
    for (int t = 0; t < THREADS; ++t)
        for (int i = 0; i < ITEMS_PER_THREAD; ++i)
            create_file(fs::path("dir" + std::to_string(t)) / ("file" + std::to_string(i)));

    std::vector<std::thread> workers;
    for (int t = 0; t < THREADS; ++t)
        workers.emplace_back([t] {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i)
                EXPECT_TRUE(RecycleBin::instance().add_item(fs::path("dir" + std::to_string(t)) / ("file" + std::to_string(i))));
        });
    for (std::thread& worker : workers)
        worker.join();

    EXPECT_EQ(RecycleBin::instance().list_items().size(), static_cast<size_t>(THREADS * ITEMS_PER_THREAD));
}

#ifdef __linux__
class RecycleBinOnAnotherFilesystemTest : public RecycleBinTest {  // [RecycleBinTests.md#RecycleBinOnAnotherFilesystemTest]
protected:
    const fs::path other_ = fs::path("/dev/shm") / ("autofilesync_recyclebin_" + std::to_string(getpid()));

    void SetUp() override
    {
        RecycleBinTest::SetUp();
        std::error_code ec;
        fs::create_directories(other_, ec);
        struct stat here{}, there{};
        if (ec || stat(root_.c_str(), &here) != 0 || stat(other_.c_str(), &there) != 0 || here.st_dev == there.st_dev)
            GTEST_SKIP() << "kein zweites beschreibbares Dateisystem unter /dev/shm";
        RecycleBin::instance().bin_path(other_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(other_, ec);
        RecycleBinTest::TearDown();
    }
};

TEST_F(RecycleBinOnAnotherFilesystemTest, FileTravelsToTheBinAndBack)  // [RecycleBinTests.md#FileTravelsToTheBinAndBack]
{
    const fs::path item = fs::path("sub") / "data.txt";
    create_file(item);
    const auto written = fs::last_write_time(item) - std::chrono::hours(24);
    fs::last_write_time(item, written);

    ASSERT_TRUE(RecycleBin::instance().add_item(item));

    EXPECT_FALSE(fs::exists(item)) << "the file must leave its place even when it cannot simply be renamed";
    EXPECT_EQ(content_of(other_ / item), "content");
    EXPECT_EQ(fs::last_write_time(other_ / item), written) << "a deleted file keeps its age in the bin";
    ASSERT_TRUE(RecycleBin::instance().restore_item(item));
    EXPECT_EQ(content_of(item), "content");
    EXPECT_EQ(fs::last_write_time(item), written) << "a restored file must not look freshly edited to the next sync";
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(RecycleBinOnAnotherFilesystemTest, DirectoryTravelsToTheBin)  // [RecycleBinTests.md#DirectoryTravelsToTheBin]
{
    create_file(fs::path("tree") / "inner" / "data.txt");

    EXPECT_TRUE(RecycleBin::instance().add_item("tree"));

    EXPECT_FALSE(fs::exists("tree"));
    EXPECT_EQ(content_of(other_ / "tree" / "inner" / "data.txt"), "content");
}
#endif

} // namespace
