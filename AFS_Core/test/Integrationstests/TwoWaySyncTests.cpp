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
#include "ScopedDeleteMode.h"
#include "FileUtil.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TwoWaySyncTests.h"
#include "Util.h"
#include <chrono>
#include <filesystem>
#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

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
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
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

TEST_F(TwoWaySyncTest, TargetOnlyDirectoryIsMirroredIntoTheSource)  // [TwoWaySyncTests.md#target-only-directories]
{
    tu::write_text_file(source_dir_ / "root.txt", "created on the source");
    tu::write_text_file(target_dir_ / "target_only" / "inside.txt", "created on the target");
    tu::write_text_file(target_dir_ / "target_only" / "deep" / "deeper.txt", "deep on the target");
    tu::write_text_file(target_dir_ / "cache" / "tmp.txt", "excluded");
    SyncItem si = two_way_item(SyncItemType::Tree);
    si.add_exclusion(L"cache");

    EXPECT_EQ(tu::run_sync(si), 3u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "target_only" / "inside.txt"), "created on the target");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "target_only" / "deep" / "deeper.txt"), "deep on the target");
    EXPECT_TRUE(fs::exists(target_dir_ / "target_only" / "inside.txt")) << "nothing on the target side may be lost";
    EXPECT_FALSE(fs::exists(source_dir_ / "cache")) << "an excluded directory does not travel back either";
    EXPECT_EQ(tu::run_sync(si), 0u);
}

} // namespace

namespace {
    SyncState fresh_state(const SyncItem& si)
    {
        return SyncState(si.source(), si.target());
    }
}

TEST_F(TwoWaySyncTest, FirstPassRecordsEveryFileInTheState)  // [TwoWaySyncTests.md#state]
{
    tu::write_text_file(source_dir_ / "same.txt", "same", seconds(3600));
    tu::write_text_file(target_dir_ / "same.txt", "same", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "new.txt", "new in the source");
    tu::write_text_file(target_dir_ / "back.txt", "new in the target");
    tu::write_text_file(target_dir_ / "cache.tmp", "excluded");
    SyncItem si = two_way_item(SyncItemType::Tree);
    si.add_exclusion(L"*.tmp");
    SyncState state = fresh_state(si);

    EXPECT_EQ(tu::run_sync(si, state), 2u);

    EXPECT_EQ(state.files().size(), 3u);
    ASSERT_NE(state.find(L"same.txt"), nullptr);
    ASSERT_NE(state.find(L"sub/new.txt"), nullptr);
    ASSERT_NE(state.find(L"back.txt"), nullptr);
    EXPECT_EQ(state.find(L"cache.tmp"), nullptr) << "an excluded file is not the state's business";
    EXPECT_EQ(state.find(L"sub/new.txt")->size, std::string("new in the source").size());
    EXPECT_TRUE(util::equal(state.find(L"sub/new.txt")->target_time, fs::last_write_time(target_dir_ / "sub" / "new.txt")));
    EXPECT_TRUE(state.changed());
}

TEST_F(TwoWaySyncTest, FileDeletedInTheSourceIsDeletedInTheTargetWhenTheStateKnowsIt)  // [TwoWaySyncTests.md#state]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "gone.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "kept.txt", "stays", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::remove(source_dir_ / "gone.txt");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(target_dir_ / "gone.txt")) << "the deletion must reach the target instead of being undone";
    EXPECT_TRUE(fs::exists(target_dir_ / "sub" / "kept.txt"));
    EXPECT_EQ(state.find(L"gone.txt"), nullptr);
    EXPECT_NE(state.find(L"sub/kept.txt"), nullptr);
}

TEST_F(TwoWaySyncTest, FileDeletedInTheTargetIsDeletedInTheSourceWhenTheStateKnowsIt)  // [TwoWaySyncTests.md#state]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "gone.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "kept.txt", "stays", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::remove(target_dir_ / "gone.txt");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(source_dir_ / "gone.txt")) << "a deletion in the target must reach the source";
    EXPECT_TRUE(fs::exists(source_dir_ / "kept.txt"));
    EXPECT_EQ(state.find(L"gone.txt"), nullptr);
}

TEST_F(TwoWaySyncTest, DeletedFileItemIsDeletedOnTheOtherSideToo)
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "single.txt", "synced once", seconds(3600));
    const SyncItem si = two_way_file_item(L"single.txt");
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    ASSERT_NE(state.find(L"single.txt"), nullptr) << "a file item is keyed by its name";
    fs::remove(target_dir_ / "single.txt");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(source_dir_ / "single.txt"));
    EXPECT_TRUE(state.empty());
}

TEST_F(TwoWaySyncTest, FileChangedAfterItsDeletionOnTheOtherSideIsCopiedNotDeleted)  // [TwoWaySyncTests.md#edit-beats-deletion]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "v1", seconds(7200));
    tu::write_text_file(source_dir_ / "b.txt", "v1", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::remove(target_dir_ / "a.txt");
    tu::write_text_file(source_dir_ / "a.txt", "edited on the source after the target deleted it");
    fs::remove(source_dir_ / "b.txt");
    tu::write_text_file(target_dir_ / "b.txt", "edited on the target after the source deleted it");

    EXPECT_EQ(tu::run_sync(si, state), 2u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "edited on the source after the target deleted it");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "b.txt"), "edited on the target after the source deleted it");
    EXPECT_EQ(state.files().size(), 2u);
}

TEST_F(TwoWaySyncTest, FileChangedOnBothSidesSinceTheLastSyncIsLeftAlone)  // [TwoWaySyncTests.md#both-modified]
{
    tu::write_text_file(source_dir_ / "c.txt", "v1", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    tu::write_text_file(source_dir_ / "c.txt", "source v2", seconds(3600));
    tu::write_text_file(target_dir_ / "c.txt", "target version 2");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "c.txt"), "source v2") << "the newer target must not win over an edited source";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "c.txt"), "target version 2");
    EXPECT_EQ(state.find(L"c.txt")->size, 2u) << "the state keeps the last agreed version until the user settles the conflict";
}

TEST_F(TwoWaySyncTest, ChangedSideWinsEvenWhenTheOtherSideCarriesTheNewerTime)  // [TwoWaySyncTests.md#changed-side-wins]
{
    tu::write_text_file(source_dir_ / "d.txt", "v1", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    tu::write_text_file(source_dir_ / "d.txt", "restored from an archive", seconds(10800));

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "d.txt"), "restored from an archive") << "without the state the newer target would have overwritten the edit";
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(TwoWaySyncTest, SameFileDroppedOnBothSidesIsNoConflict)
{
    tu::write_text_file(source_dir_ / "e.txt", "v1", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    tu::write_text_file(source_dir_ / "e.txt", "v2 on both", seconds(3600));
    tu::write_text_file(target_dir_ / "e.txt", "v2 on both", seconds(3600));

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_EQ(state.find(L"e.txt")->size, std::string("v2 on both").size()) << "the state follows the sides that agree";
}

TEST_F(TwoWaySyncTest, DirectoryDeletedInTheSourceIsDeletedInTheTarget)  // [TwoWaySyncTests.md#deleted-directories]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "a", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "deep" / "b.txt", "b", seconds(3600));
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 3u);
    fs::remove_all(source_dir_ / "sub");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(target_dir_ / "sub")) << "the whole directory must go, not only its files";
    EXPECT_TRUE(fs::exists(target_dir_ / "keep.txt"));
    EXPECT_FALSE(state.knows_below(L"sub"));
    EXPECT_NE(state.find(L"keep.txt"), nullptr);
    EXPECT_EQ(tu::run_sync(si, state), 0u);
    EXPECT_FALSE(fs::exists(source_dir_ / "sub")) << "the deleted directory must not come back";
}

TEST_F(TwoWaySyncTest, DirectoryDeletedInTheTargetIsDeletedInTheSource)  // [TwoWaySyncTests.md#deleted-directories]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "a", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "deep" / "b.txt", "b", seconds(3600));
    tu::write_text_file(source_dir_ / "keep.txt", "stays", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 3u);
    fs::remove_all(target_dir_ / "sub");

    EXPECT_EQ(tu::run_sync(si, state), 0u);

    EXPECT_FALSE(fs::exists(source_dir_ / "sub")) << "a directory deleted in the target must leave the source";
    EXPECT_FALSE(fs::exists(target_dir_ / "sub")) << "the pass must not recreate the deleted directory in the target";
    EXPECT_TRUE(fs::exists(source_dir_ / "keep.txt"));
    EXPECT_FALSE(state.knows_below(L"sub"));
}

TEST_F(TwoWaySyncTest, EditedFileSurvivesTheDeletionOfItsDirectory)  // [TwoWaySyncTests.md#deleted-directories]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "sub" / "a.txt", "a", seconds(7200));
    tu::write_text_file(source_dir_ / "sub" / "deep" / "b.txt", "b", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::remove_all(source_dir_ / "sub");
    tu::write_text_file(target_dir_ / "sub" / "deep" / "b.txt", "edited after the deletion");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(source_dir_ / "sub" / "deep" / "b.txt"), "edited after the deletion") << "the edit must come back, directory and all";
    EXPECT_FALSE(fs::exists(target_dir_ / "sub" / "a.txt")) << "the untouched file follows the deletion";
    EXPECT_TRUE(fs::exists(target_dir_ / "sub" / "deep" / "b.txt"));
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(TwoWaySyncTest, NewFilesInsideADirectoryDeletedOnTheOtherSideStillTravel)  // [TwoWaySyncTests.md#deleted-directories]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "sub" / "old.txt", "old", seconds(7200));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    fs::remove_all(target_dir_ / "sub");
    tu::write_text_file(source_dir_ / "sub" / "new" / "x.txt", "added later");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "sub" / "new" / "x.txt"), "added later");
    EXPECT_FALSE(fs::exists(source_dir_ / "sub" / "old.txt")) << "the deletion still reaches the old file";
    EXPECT_FALSE(fs::exists(target_dir_ / "sub" / "old.txt"));
}

namespace {
    uint64_t inode_of(const fs::path& file)  // [TwoWaySyncTests.md#renames]
    {
#ifdef _WIN32
        return 0;
#else
        struct stat st{};
        return ::stat(file.c_str(), &st) == 0 ? st.st_ino : 0;
#endif
    }
}

TEST_F(TwoWaySyncTest, RenamedSourceFileIsRenamedInTheTarget)  // [TwoWaySyncTests.md#renames]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "old name.txt", "payload that must not travel again", seconds(3600));
    tu::write_text_file(source_dir_ / "other.txt", "other", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    const uint64_t inode = inode_of(target_dir_ / "old name.txt");
    fs::rename(source_dir_ / "old name.txt", source_dir_ / "new name.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u) << "the rename counts as one step";

    EXPECT_EQ(tu::read_text_file(target_dir_ / "new name.txt"), "payload that must not travel again");
    EXPECT_FALSE(fs::exists(target_dir_ / "old name.txt"));
    EXPECT_EQ(inode_of(target_dir_ / "new name.txt"), inode) << "the target file was copied, not renamed";
    EXPECT_EQ(state.find(L"old name.txt"), nullptr);
    ASSERT_NE(state.find(L"new name.txt"), nullptr);
    EXPECT_EQ(tu::run_sync(si, state), 0u);
}

TEST_F(TwoWaySyncTest, MovedSourceFileIsMovedInTheTarget)  // [TwoWaySyncTests.md#renames]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "moved into a new directory", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Tree);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    const uint64_t inode = inode_of(target_dir_ / "a.txt");
    fs::create_directories(source_dir_ / "sub" / "deep");
    fs::rename(source_dir_ / "a.txt", source_dir_ / "sub" / "deep" / "a.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(inode_of(target_dir_ / "sub" / "deep" / "a.txt"), inode);
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
    EXPECT_NE(state.find(L"sub/deep/a.txt"), nullptr);
}

TEST_F(TwoWaySyncTest, RenamedTargetFileIsRenamedInTheSource)  // [TwoWaySyncTests.md#renames]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "z.txt", "renamed on the target side", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    const uint64_t inode = inode_of(source_dir_ / "z.txt");
    fs::rename(target_dir_ / "z.txt", target_dir_ / "a.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(inode_of(source_dir_ / "a.txt"), inode);
    EXPECT_FALSE(fs::exists(source_dir_ / "z.txt"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "renamed on the target side");
}

TEST_F(TwoWaySyncTest, TwoFilesAlikeAreCopiedNotRenamed)  // [TwoWaySyncTests.md#renames-ambiguous]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "same size", seconds(3600));
    tu::write_text_file(source_dir_ / "b.txt", "same size", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::rename(source_dir_ / "a.txt", source_dir_ / "c.txt");

    tu::run_sync(si, state);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "c.txt"), "same size");
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
    EXPECT_TRUE(fs::exists(target_dir_ / "b.txt")) << "the look-alike must stay";
    EXPECT_EQ(state.files().size(), 2u);
}

TEST_F(TwoWaySyncTest, RenamedAndEditedFileIsCopied)
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "v1", seconds(3600));
    const SyncItem si = two_way_item(SyncItemType::Dir);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    fs::rename(source_dir_ / "a.txt", source_dir_ / "b.txt");
    tu::write_text_file(source_dir_ / "b.txt", "v2, longer");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "b.txt"), "v2, longer");
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt")) << "the old name is a deletion";
}

TEST_F(TwoWaySyncTest, OneWayItemRenamesInTheTargetWhenItTracksMoves)  // [TwoWaySyncTests.md#one-way-moves]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "one-way payload", seconds(3600));
    SyncItem si = two_way_item(SyncItemType::Tree);
    si.two_way_sync(false);
    si.delete_in_target(true);
    si.track_moves(true);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    const uint64_t inode = inode_of(target_dir_ / "a.txt");
    fs::rename(source_dir_ / "a.txt", source_dir_ / "b.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(inode_of(target_dir_ / "b.txt"), inode);
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
}

TEST_F(TwoWaySyncTest, OneWayItemWithAStateStillNeverDeletesInTheSource)  // [TwoWaySyncTests.md#one-way-moves]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "master copy", seconds(3600));
    tu::write_text_file(source_dir_ / "b.txt", "kept in the target", seconds(3600));
    SyncItem si = two_way_item(SyncItemType::Tree);
    si.two_way_sync(false);
    si.track_moves(true);
    SyncState state = fresh_state(si);
    ASSERT_EQ(tu::run_sync(si, state), 2u);
    fs::remove(target_dir_ / "a.txt");
    fs::remove(source_dir_ / "b.txt");

    EXPECT_EQ(tu::run_sync(si, state), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "master copy") << "a one-way item copies a deleted target again";
    EXPECT_TRUE(fs::exists(target_dir_ / "b.txt")) << "without delete_in_target the target keeps the file";
    EXPECT_FALSE(fs::exists(source_dir_ / "b.txt"));
}
