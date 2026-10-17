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
#include "SyncDir.h"
#include "SyncFile.h"
#include "SyncPlan.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <thread>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

std::set<std::string> entries_below(const fs::path& dir)
{
    std::set<std::string> entries;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(dir))
        entries.insert(entry.path().lexically_relative(dir).generic_string());
    return entries;
}

fs::path unmounted_dir(const fs::path& root)  // [SyncPlanTests.md#unmounted_dir]
{
#ifdef _WIN32
    std::error_code ec;
    for (wchar_t letter = L'Z'; letter > L'C'; --letter)
        if (const fs::path drive = std::wstring{letter, L':', L'\\'}; !fs::exists(drive, ec))
            return drive / "not-mounted";
#endif
    return root / "not-mounted";
}

class SyncPlanTest: public ::testing::Test {
protected:
    const fs::path root_{TestPaths::scratch_dir() + L"/plan"};
    const fs::path source_dir_ = root_ / "source";
    const fs::path target_dir_ = root_ / "target";
    const fs::path unmounted_dir_ = unmounted_dir(root_);

    void SetUp() override
    {
        remove_root();
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override { remove_root(); }

    void remove_root() const
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem item(SyncItemType type) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        si.name(L"plan");
        return si;
    }

    static SyncPlan preview(const SyncItem& si, std::size_t& count)  // [SyncPlanTests.md#preview]
    {
        SyncPlan plan;
        SyncPlanGuard guard(&plan);
        switch (si.sync_item_type()) {
        case SyncItemType::Tree: { SyncTree tree(si, count, true); tree(); break; }
        case SyncItemType::Dir: { SyncDir dir(si, count, true); dir(); break; }
        default: { SyncFile file(si, count, true); file(); break; }
        }
        return plan;
    }

    static SyncPlan preview(const SyncItem& si)
    {
        std::size_t count = 0;
        return preview(si, count);
    }
};

const PlannedStep* step_for(const SyncPlan& plan, const std::string& file_name)
{
    for (const PlannedStep& step : plan.steps())
        if (fs::path(step.source).filename() == file_name || fs::path(step.target).filename() == file_name)
            return &step;
    return nullptr;
}

}

TEST_F(SyncPlanTest, PreviewListsCopyAndDeletionAndTouchesNothing)  // [SyncPlanTests.md#PreviewListsCopyAndDeletionAndTouchesNothing]
{
    tu::write_text_file(source_dir_ / "new.txt", "new");
    tu::write_text_file(source_dir_ / "sub" / "changed.txt", "changed");
    tu::write_text_file(target_dir_ / "sub" / "changed.txt", "old", seconds(3600));
    tu::write_text_file(target_dir_ / "gone.txt", "only in the target");
    const auto before = entries_below(root_);
    SyncItem si = item(SyncItemType::Tree);
    si.delete_in_target(true);

    std::size_t count = 0;
    const SyncPlan plan = preview(si, count);

    EXPECT_EQ(entries_below(root_), before) << "the preview changed the filesystem";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "sub" / "changed.txt"), "old");
    EXPECT_EQ(plan.count(), 3u);
    EXPECT_EQ(count, 3u) << "the preview must count what it plans";
    EXPECT_EQ(plan.count(PlannedAction::CopyToTarget), 2u);
    EXPECT_EQ(plan.count(PlannedAction::DeleteInTarget), 1u);
    EXPECT_EQ(plan.bytes(), 3u + 7u) << "only the copies move bytes";
    ASSERT_NE(step_for(plan, "gone.txt"), nullptr);
    EXPECT_EQ(step_for(plan, "gone.txt")->action, PlannedAction::DeleteInTarget);
    EXPECT_EQ(step_for(plan, "gone.txt")->item, L"plan");
    ASSERT_NE(step_for(plan, "changed.txt"), nullptr);
    EXPECT_EQ(fs::path(step_for(plan, "changed.txt")->target), target_dir_ / "sub" / "changed.txt");
}

TEST_F(SyncPlanTest, TwoWayPreviewPlansTheCopyBack)
{
    tu::write_text_file(source_dir_ / "notes.txt", "older", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "newer on the target");
    tu::write_text_file(target_dir_ / "target-only.txt", "x");
    SyncItem si = item(SyncItemType::Dir);
    si.two_way_sync(true);

    const SyncPlan plan = preview(si);

    EXPECT_EQ(plan.count(PlannedAction::CopyToSource), 2u);
    EXPECT_EQ(plan.count(), 2u);
    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "older");
    EXPECT_FALSE(fs::exists(source_dir_ / "target-only.txt"));
}

TEST_F(SyncPlanTest, DeleteAfterCopyIsPlannedAsAMove)
{
    tu::write_text_file(source_dir_ / "move-me.txt", "content");
    SyncItem si = item(SyncItemType::Dir);
    si.del_aftercopy(true);

    const SyncPlan plan = preview(si);

    EXPECT_EQ(plan.count(PlannedAction::MoveToTarget), 1u);
    EXPECT_TRUE(fs::exists(source_dir_ / "move-me.txt")) << "the preview must not move anything";
}

TEST_F(SyncPlanTest, SingleFileItemIsPlannedToo)
{
    tu::write_text_file(source_dir_ / "one.txt", "1");
    SyncItem si = item(SyncItemType::File);
    si.source((source_dir_ / "one.txt").wstring());
    si.target((target_dir_ / "one.txt").wstring());

    const SyncPlan plan = preview(si);

    ASSERT_EQ(plan.steps().size(), 1u);
    EXPECT_EQ(plan.steps().front().action, PlannedAction::CopyToTarget);
    EXPECT_EQ(fs::path(plan.steps().front().target), target_dir_ / "one.txt");
    EXPECT_FALSE(fs::exists(target_dir_ / "one.txt"));
}

TEST_F(SyncPlanTest, EmptySourceFileIsNotPlannedUnlessWanted)  // [SyncPlanTests.md#EmptySourceFileIsNotPlannedUnlessWanted]
{
    tu::write_text_file(source_dir_ / "empty.txt", "");
    SyncItem si = item(SyncItemType::Dir);

    std::size_t count = 0;
    EXPECT_TRUE(preview(si, count).empty());
    EXPECT_EQ(count, 0u);

    si.sync_empty_files(true);
    EXPECT_EQ(preview(si).count(PlannedAction::CopyToTarget), 1u);
}

TEST_F(SyncPlanTest, UnchangedFilesAreNotPlanned)
{
    tu::write_text_file(source_dir_ / "same.txt", "same");
    fs::copy_file(source_dir_ / "same.txt", target_dir_ / "same.txt");
    fs::last_write_time(target_dir_ / "same.txt", fs::last_write_time(source_dir_ / "same.txt"));

    EXPECT_TRUE(preview(item(SyncItemType::Dir)).empty());
}

TEST_F(SyncPlanTest, CountingWithoutAPlanStillCountsEveryFile)  // [SyncPlanTests.md#CountingWithoutAPlanStillCountsEveryFile]
{
    tu::write_text_file(source_dir_ / "same.txt", "same");
    fs::copy_file(source_dir_ / "same.txt", target_dir_ / "same.txt");
    fs::last_write_time(target_dir_ / "same.txt", fs::last_write_time(source_dir_ / "same.txt"));

    std::size_t count = 0;
    SyncDir dir(item(SyncItemType::Dir), count, true);
    dir();

    EXPECT_EQ(count, 1u);
}

TEST_F(SyncPlanTest, PreviewPassNamesTheUnreachableItems)  // [SyncPlanTests.md#PreviewPassNamesTheUnreachableItems]
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    Config::instance().reset();
    Config::instance().add(SyncItem(L"unplugged", SyncItemType::Dir, source_dir_.wstring(), unmounted_dir_.wstring(), L"*"));
    Config::instance().add(SyncItem(L"same", SyncItemType::Dir, source_dir_.wstring(), target_dir_.wstring(), L"*"));
    tu::write_text_file(target_dir_ / "a.txt", "a");
    fs::last_write_time(target_dir_ / "a.txt", fs::last_write_time(source_dir_ / "a.txt"));
    Synchronize* sync = Synchronize::create(SyncType::Preview);
    ASSERT_NE(sync, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const std::vector<UnreachableItem> unreachable = sync->unreachable();
    Synchronize::destroy();
    Config::instance().reset();

    ASSERT_EQ(unreachable.size(), 1u) << "the item without a target must be named, the comparable one not";
    EXPECT_EQ(unreachable.front().name, L"unplugged");
    EXPECT_TRUE(unreachable.front().warn) << "the warning is on unless the item switches it off";
    EXPECT_NE(unreachable.front().message.find(L"not-mounted"), std::wstring::npos) << "the message must carry the path";
}

TEST_F(SyncPlanTest, UnreachableItemCarriesItsWarningChoice)
{
    Config::instance().reset();
    SyncItem backup(L"backup", SyncItemType::Dir, source_dir_.wstring(), unmounted_dir_.wstring(), L"*");
    backup.warn_if_unreachable(false);
    Config::instance().add(backup);
    Synchronize* sync = Synchronize::create(SyncType::Single);
    ASSERT_NE(sync, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const std::vector<UnreachableItem> unreachable = sync->unreachable();
    const std::optional<LastRun> run = Config::instance().syncitems().at(0).last_run();
    Synchronize::destroy();
    Config::instance().reset();

    ASSERT_EQ(unreachable.size(), 1u);
    EXPECT_FALSE(unreachable.front().warn);
    ASSERT_TRUE(run.has_value());
    EXPECT_FALSE(run->reachable);
    EXPECT_EQ(run->errors, 0u) << "an unreachable side is not an error";
}

TEST(SyncPlan, ListStopsAtMaxStepsWhileTheCountsGoOn)  // [SyncPlanTests.md#ListStopsAtMaxStepsWhileTheCountsGoOn]
{
    SyncPlan plan;
    for (std::size_t i = 0; i < SyncPlan::MAX_STEPS + 5; ++i)
        plan.add({L"item", PlannedAction::CopyToTarget, L"s", L"t", 2});
    plan.add({L"item", PlannedAction::DeleteInTarget, L"s", L"t", 100});

    EXPECT_EQ(plan.steps().size(), SyncPlan::MAX_STEPS);
    EXPECT_EQ(plan.count(), SyncPlan::MAX_STEPS + 6);
    EXPECT_EQ(plan.not_listed(), 6u);
    EXPECT_EQ(plan.bytes(), 2 * (SyncPlan::MAX_STEPS + 5)) << "a deletion moves no bytes";
}
