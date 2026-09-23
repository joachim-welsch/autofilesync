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
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TwoWaySyncTests.h"
#include "Util.h"
#include <chrono>
#include <filesystem>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

class ScopedDeleteMode {  // [TwoWaySyncTests.md#ScopedDeleteMode]
    const DeleteMode previous_ = Config::instance().settings().delete_mode();
public:
    explicit ScopedDeleteMode(DeleteMode mode) { Config::instance().settings().delete_mode(mode); }
    ~ScopedDeleteMode() { Config::instance().settings().delete_mode(previous_); }
    ScopedDeleteMode(const ScopedDeleteMode&) = delete;
    ScopedDeleteMode& operator=(const ScopedDeleteMode&) = delete;
};

class TwoWaySyncTest: public ::testing::Test {
protected:
    const fs::path source_dir_{TwoWaySyncTests::SOURCE_PATH};
    const fs::path target_dir_{TwoWaySyncTests::TARGET_PATH};

    void SetUp() override
    {
        remove_test_dirs();
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();  // [TwoWaySyncTests.md#synchronize-start]
    }

    void TearDown() override { remove_test_dirs(); }

    void remove_test_dirs() const
    {
        std::error_code ec;
        fs::remove_all(source_dir_, ec);
        fs::remove_all(target_dir_, ec);
    }

    SyncItem two_way_item(SyncItemType type) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        si.two_way_sync(true);
        return si;
    }

    SyncItem two_way_file_item(const std::wstring& filename) const
    {
        SyncItem si = two_way_item(SyncItemType::File);
        si.source((source_dir_ / filename).wstring());
        si.target((target_dir_ / filename).wstring());
        return si;
    }
};

TEST_F(TwoWaySyncTest, NewerTargetFileIsCopiedBackToTheSource)
{
    tu::write_text_file(source_dir_ / "notes.txt", "written on the source", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "edited on the target");

    EXPECT_EQ(tu::run_sync(two_way_file_item(L"notes.txt")), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "edited on the target");
    EXPECT_EQ(tu::read_text_file(target_dir_ / "notes.txt"), "edited on the target") << "the side that won must stay untouched";
    EXPECT_TRUE(util::equal(fs::last_write_time(source_dir_ / "notes.txt"),
                            fs::last_write_time(target_dir_ / "notes.txt")))
        << "the back copy must carry the target's modification time, otherwise the next pass syncs again";  // [TwoWaySyncTests.md#back-copy-timestamp]
}

TEST_F(TwoWaySyncTest, NewerSourceFileIsStillCopiedToTheTarget)
{
    tu::write_text_file(source_dir_ / "notes.txt", "edited on the source");
    tu::write_text_file(target_dir_ / "notes.txt", "written on the target", seconds(3600));

    EXPECT_EQ(tu::run_sync(two_way_file_item(L"notes.txt")), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "notes.txt"), "edited on the source");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "edited on the source");
}

TEST_F(TwoWaySyncTest, IdenticalFilesAreCopiedInNeitherDirection)
{
    tu::write_text_file(source_dir_ / "notes.txt", "same content", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "same content", seconds(1800));
    const auto source_time = fs::last_write_time(source_dir_ / "notes.txt");
    const auto target_time = fs::last_write_time(target_dir_ / "notes.txt");

    EXPECT_EQ(tu::run_sync(two_way_file_item(L"notes.txt")), 0u);

    EXPECT_TRUE(util::equal(fs::last_write_time(source_dir_ / "notes.txt"), source_time));
    EXPECT_TRUE(util::equal(fs::last_write_time(target_dir_ / "notes.txt"), target_time));
}

TEST_F(TwoWaySyncTest, FileDeletedInTheSourceIsRestoredFromTheTarget)
{
    tu::write_text_file(target_dir_ / "restored.txt", "still on the target");

    EXPECT_EQ(tu::run_sync(two_way_file_item(L"restored.txt")), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "restored.txt"), "still on the target");
    EXPECT_TRUE(fs::exists(target_dir_ / "restored.txt"));
}

TEST_F(TwoWaySyncTest, DeleteInTargetWinsOverTwoWaySyncWhenTheSourceFileIsGone)  // [TwoWaySyncTests.md#DeleteInTargetWinsOverTwoWaySyncWhenTheSourceFileIsGone]
{
    const ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(target_dir_ / "orphan.txt", "only on the target");

    SyncItem si = two_way_file_item(L"orphan.txt");
    si.delete_in_target(true);
    tu::run_sync(si);

    EXPECT_FALSE(fs::exists(target_dir_ / "orphan.txt"));
    EXPECT_FALSE(fs::exists(source_dir_ / "orphan.txt")) << "the deleted file must not come back through the two-way direction";
}

TEST_F(TwoWaySyncTest, EmptyTargetOnlyFileIsSkippedUntilSyncEmptyFilesIsSet)  // [TwoWaySyncTests.md#EmptyTargetOnlyFileIsSkippedUntilSyncEmptyFilesIsSet]
{
    tu::write_text_file(target_dir_ / "empty.txt", "");

    SyncItem si = two_way_item(SyncItemType::Dir);
    EXPECT_EQ(tu::run_sync(si), 0u);
    EXPECT_FALSE(fs::exists(source_dir_ / "empty.txt"));

    si.sync_empty_files(true);
    EXPECT_EQ(tu::run_sync(si), 1u);
    EXPECT_TRUE(fs::exists(source_dir_ / "empty.txt"));
}

TEST_F(TwoWaySyncTest, DirSyncExchangesTheNewerVersionOfEveryFileInOnePass)  // [TwoWaySyncTests.md#DirSyncExchangesTheNewerVersionOfEveryFileInOnePass]
{
    tu::write_text_file(source_dir_ / "only_source.txt", "created on the source");
    tu::write_text_file(target_dir_ / "only_target.txt", "created on the target");
    tu::write_text_file(source_dir_ / "source_wins.txt", "newer source version");
    tu::write_text_file(target_dir_ / "source_wins.txt", "older target version", seconds(3600));
    tu::write_text_file(source_dir_ / "target_wins.txt", "older source version", seconds(3600));
    tu::write_text_file(target_dir_ / "target_wins.txt", "newer target version");

    tu::run_sync(two_way_item(SyncItemType::Dir));

    EXPECT_EQ(tu::read_text_file(target_dir_ / "only_source.txt"), "created on the source");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "only_target.txt"), "created on the target");
    EXPECT_EQ(tu::read_text_file(target_dir_ / "source_wins.txt"), "newer source version");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "target_wins.txt"), "newer target version");
    EXPECT_TRUE(fs::exists(target_dir_ / "only_target.txt")) << "two-way sync must never delete, only add";
}

TEST_F(TwoWaySyncTest, RepeatedDirSyncCopiesNothingMore)  // [TwoWaySyncTests.md#idempotence]
{
    tu::write_text_file(source_dir_ / "only_source.txt", "created on the source");
    tu::write_text_file(target_dir_ / "only_target.txt", "created on the target");
    tu::write_text_file(source_dir_ / "shared.txt", "older source version", seconds(3600));
    tu::write_text_file(target_dir_ / "shared.txt", "newer target version");

    const SyncItem si = two_way_item(SyncItemType::Dir);
    EXPECT_EQ(tu::run_sync(si), 3u);
    EXPECT_EQ(tu::run_sync(si), 0u) << "a settled pair of directories must not keep copying files back and forth";
    EXPECT_EQ(tu::read_text_file(source_dir_ / "shared.txt"), "newer target version");
}

TEST_F(TwoWaySyncTest, BackCopyLeavesNoBackupFileBehindInTheSource)  // [TwoWaySyncTests.md#BackCopyLeavesNoBackupFileBehindInTheSource]
{
    tu::write_text_file(source_dir_ / "shared.txt", "older source version", seconds(3600));
    tu::write_text_file(target_dir_ / "shared.txt", "newer target version");

    tu::run_sync(two_way_item(SyncItemType::Dir));

    EXPECT_FALSE(fs::exists(source_dir_ / "shared.txt.bak"))
        << "the transient backup of save_copy_file() must be gone once the copy succeeded";
}

TEST_F(TwoWaySyncTest, ExcludedTargetOnlyFileIsNotCopiedBackToTheSource)
{
    tu::write_text_file(target_dir_ / "cache.tmp", "temporary target data");
    tu::write_text_file(target_dir_ / "keep.txt", "wanted on both sides");

    SyncItem si = two_way_item(SyncItemType::Dir);
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    si.exclusions(exclusions);
    tu::run_sync(si);

    EXPECT_FALSE(fs::exists(source_dir_ / "cache.tmp"));
    EXPECT_TRUE(fs::exists(source_dir_ / "keep.txt"));
}

TEST_F(TwoWaySyncTest, WildcardAlsoFiltersTheTargetToSourceDirection)
{
    tu::write_text_file(target_dir_ / "notes.log", "not covered by the wildcard");
    tu::write_text_file(target_dir_ / "notes.txt", "covered by the wildcard");

    SyncItem si = two_way_item(SyncItemType::Dir);
    si.wildcard(L"*.txt");
    tu::run_sync(si);

    EXPECT_FALSE(fs::exists(source_dir_ / "notes.log"));
    EXPECT_TRUE(fs::exists(source_dir_ / "notes.txt"));
}

TEST_F(TwoWaySyncTest, TreeSyncMergesBothSidesAtEveryLevel)
{
    tu::write_text_file(source_dir_ / "sub" / "only_source.txt", "created on the source");
    tu::write_text_file(target_dir_ / "sub" / "only_target.txt", "created on the target");
    tu::write_text_file(source_dir_ / "sub" / "deep" / "target_wins.txt", "older source version", seconds(3600));
    tu::write_text_file(target_dir_ / "sub" / "deep" / "target_wins.txt", "newer target version");

    tu::run_sync(two_way_item(SyncItemType::Tree));

    EXPECT_EQ(tu::read_text_file(target_dir_ / "sub" / "only_source.txt"), "created on the source");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "sub" / "only_target.txt"), "created on the target");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "sub" / "deep" / "target_wins.txt"), "newer target version");
}

TEST_F(TwoWaySyncTest, RepeatedTreeSyncCopiesNothingMore)  // [TwoWaySyncTests.md#idempotence]
{
    tu::write_text_file(source_dir_ / "sub" / "only_source.txt", "created on the source");
    tu::write_text_file(target_dir_ / "sub" / "only_target.txt", "created on the target");
    tu::write_text_file(source_dir_ / "sub" / "shared.txt", "older source version", seconds(3600));
    tu::write_text_file(target_dir_ / "sub" / "shared.txt", "newer target version");

    const SyncItem si = two_way_item(SyncItemType::Tree);
    EXPECT_EQ(tu::run_sync(si), 3u);
    EXPECT_EQ(tu::run_sync(si), 0u);
}

TEST_F(TwoWaySyncTest, TargetOnlyDirectoryIsLeftAloneByTwoWaySync)  // [TwoWaySyncTests.md#TargetOnlyDirectoryIsLeftAloneByTwoWaySync]
{
    tu::write_text_file(source_dir_ / "root.txt", "created on the source");
    tu::write_text_file(target_dir_ / "target_only" / "inside.txt", "created on the target");

    tu::run_sync(two_way_item(SyncItemType::Tree));

    EXPECT_TRUE(fs::exists(target_dir_ / "target_only" / "inside.txt")) << "nothing on the target side may be lost";
    EXPECT_FALSE(fs::exists(source_dir_ / "target_only"));
}

} // namespace
