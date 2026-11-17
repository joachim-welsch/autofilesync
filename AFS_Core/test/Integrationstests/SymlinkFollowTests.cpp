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
#include "CopyLog.h"
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "ScopedLog.h"
#include "StrUtil.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include "Logging.h"
#include <SymlinkUtil.h>
#include <filesystem>
#include <fstream>
#include <set>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class SymlinkFollowTest : public ::testing::Test {  // [SymlinkFollowTests.md#SymlinkFollowTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("afs_follow_" + std::to_string(TestPaths::process_id()));
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";
    const fs::path outside_ = root_ / "fremd";
    const tu::ScopedLog log_{fs::temp_directory_path() / ("autofilesync_follow_" + std::to_string(TestPaths::process_id()) + ".log"), LogLevel::Info};

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_ / "echt");
        fs::create_directories(outside_ / "tief");
        fs::create_directories(target_);
        tu::write_text_file(source_ / "echt" / "a.txt", "a");
        tu::write_text_file(source_ / "b.txt", "b");
        tu::write_text_file(outside_ / "f.txt", "f");
        tu::write_text_file(outside_ / "tief" / "t.txt", "t");
        Synchronize::start();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem item(SyncItemType type = SyncItemType::Tree) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source_.wstring());
        si.target(target_.wstring());
        si.symlink_mode(SymlinkMode::Follow);
        si.name(L"folgen " + util::string_to_wstring(testing::UnitTest::GetInstance()->current_test_info()->name()));  // [SymlinkFollowTests.md#SymlinkFollowTest]
        return si;
    }

    static void run(const SyncItem& si, Outcome& outcome)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si);
    }

    int logged(const char* message_id) const  // [SymlinkTests.md#skipped_links]
    {
        std::ifstream in(log_.path());
        const std::string message = translation::translate_utf8(message_id);
        int count = 0;
        for (std::string line; std::getline(in, line);)
            if (line.find(message) != std::string::npos)
                ++count;
        return count;
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }
};

} // namespace

TEST_F(SymlinkFollowTest, DirectoryLinkIsEnteredAndItsContentCopied)  // [SymlinkFollowTests.md#DirectoryLinkIsEnteredAndItsContentCopied]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_symlink("../fremd/f.txt", source_ / "datei.txt");
    Outcome first, second;

    run(item(), first);

    EXPECT_TRUE(first.report.successful());
    EXPECT_EQ(first.copies, 5u) << "two files of the tree, two behind the directory link and the file link";
    EXPECT_FALSE(fs::is_symlink(target_ / "raus")) << "the target gets a directory, not a link";
    EXPECT_EQ(tu::read_text_file(target_ / "raus" / "f.txt"), "f");
    EXPECT_EQ(tu::read_text_file(target_ / "raus" / "tief" / "t.txt"), "t");
    EXPECT_FALSE(fs::is_symlink(target_ / "datei.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "datei.txt"), "f");

    run(item(), second);

    EXPECT_EQ(second.copies, 0u);
}

TEST_F(SymlinkFollowTest, LinkBackIntoTheOwnWayIsNotEnteredAndNamedOnce)  // [SymlinkFollowTests.md#loops]
{
    fs::create_directory_symlink("..", source_ / "echt" / "hoch");
    fs::create_directory_symlink(".", source_ / "selbst");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful()) << "a loop is skipped, not failed at";
    EXPECT_EQ(outcome.copies, 2u);
    EXPECT_FALSE(fs::exists(target_ / "echt" / "hoch"));
    EXPECT_FALSE(fs::exists(target_ / "selbst"));
    EXPECT_EQ(logged("links_loop_skipped"), 1) << "one line for both links";
}

TEST_F(SymlinkFollowTest, LoopThatClosesOutsideTheTreeIsFound)  // [SymlinkFollowTests.md#loops]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_directory_symlink(source_, outside_ / "tief" / "zurueck");
    fs::create_directory_symlink(outside_, outside_ / "kreis");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_EQ(tu::read_text_file(target_ / "raus" / "tief" / "t.txt"), "t");
    EXPECT_FALSE(fs::exists(target_ / "raus" / "tief" / "zurueck")) << "it leads back to the root of the walk";
    EXPECT_FALSE(fs::exists(target_ / "raus" / "kreis")) << "it leads back to the directory the walk is in";
    EXPECT_EQ(outcome.copies, 4u);
}

TEST_F(SymlinkFollowTest, LinkIntoTheTargetIsNotEntered)  // [SymlinkFollowTests.md#LinkIntoTheTargetIsNotEntered]
{
    fs::create_directory_symlink(target_, source_ / "sicherung");
    Outcome first, second;

    run(item(), first);
    run(item(), second);

    EXPECT_TRUE(second.report.successful());
    EXPECT_FALSE(fs::exists(target_ / "sicherung")) << "the copy would be read as source again, pass after pass";
    EXPECT_EQ(second.copies, 0u);
    EXPECT_EQ(logged("links_into_target_skipped"), 1);
}

TEST_F(SymlinkFollowTest, TwoLinksToTheSameDirectoryAreBothEntered)  // [SymlinkFollowTests.md#TwoLinksToTheSameDirectoryAreBothEntered]
{
    fs::create_directory_symlink("../fremd", source_ / "eins");
    fs::create_directory_symlink("../fremd", source_ / "zwei");
    fs::create_directory_symlink("echt", source_ / "kopie");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_EQ(tu::read_text_file(target_ / "eins" / "f.txt"), "f");
    EXPECT_EQ(tu::read_text_file(target_ / "zwei" / "f.txt"), "f") << "visited before is no loop";
    EXPECT_EQ(tu::read_text_file(target_ / "kopie" / "a.txt"), "a") << "a link into the own tree doubles the content, as asked";
    EXPECT_EQ(logged("links_loop_skipped"), 0);
}

TEST_F(SymlinkFollowTest, ChainOfLinksIsCutAtTheLimit)  // [SymlinkFollowTests.md#ChainOfLinksIsCutAtTheLimit]
{
    fs::path here = source_;
    for (int i = 0; i <= symlink_util::MAX_FOLLOWED_LINKS; ++i) {
        const fs::path next = root_ / ("kette" + std::to_string(i));
        fs::create_directories(next);
        tu::write_text_file(next / "hier.txt", std::to_string(i));
        fs::create_directory_symlink(next, here / "weiter");
        here = next;
    }
    Outcome outcome;

    run(item(), outcome);

    fs::path reached = target_;
    for (int i = 0; i < symlink_util::MAX_FOLLOWED_LINKS; ++i)
        reached /= "weiter";
    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(reached / "hier.txt"), std::to_string(symlink_util::MAX_FOLLOWED_LINKS - 1)) << "the last link that is still followed";
    EXPECT_FALSE(fs::exists(reached / "weiter")) << "one link more than the limit";
    EXPECT_EQ(logged("links_too_deep_skipped"), 1);
}

TEST_F(SymlinkFollowTest, LinkThatLeadsNowhereIsSkippedAndCounted)  // [SymlinkFollowTests.md#LinkThatLeadsNowhereIsSkippedAndCounted]
{
    fs::create_directory_symlink("../weg", source_ / "tot");
    tu::write_text_file(target_ / "tot", "liegt schon da");
    SyncItem si = item();
    si.delete_in_target(true);
    Outcome outcome;

    run(si, outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(target_ / "tot"), "liegt schon da") << "a dead link is no deleted entry";
    EXPECT_EQ(logged("links_dangling_skipped"), 1);
}

TEST_F(SymlinkFollowTest, DeleteInTargetWorksBehindAFollowedLink)  // [SymlinkFollowTests.md#DeleteInTargetWorksBehindAFollowedLink]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    fs::create_directory_symlink("../fremd", source_ / "raus");
    tu::write_text_file(target_ / "raus" / "alt.txt", "nur im Ziel");
    SyncItem si = item();
    si.delete_in_target(true);
    Outcome outcome;

    run(si, outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_FALSE(fs::exists(target_ / "raus" / "alt.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "raus" / "f.txt"), "f");
    EXPECT_EQ(tu::read_text_file(outside_ / "f.txt"), "f") << "nothing behind the link is touched";
}

TEST_F(SymlinkFollowTest, ExcludedLinkIsNotEntered)  // [SymlinkFollowTests.md#ExcludedLinkIsNotEntered]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    SyncItem si = item();
    Exclusions exclusions;
    exclusions.push_back(L"raus");
    si.exclusions(exclusions);
    Outcome outcome;

    run(si, outcome);

    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt"}));
}

TEST_F(SymlinkFollowTest, PreviewCountsBehindTheLinkAndCreatesNothing)  // [SymlinkFollowTests.md#PreviewCountsBehindTheLinkAndCreatesNothing]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_directory_symlink(".", source_ / "selbst");
    std::size_t planned = 0;

    SyncTree preview(item(), planned, true);
    preview();

    EXPECT_EQ(planned, 4u) << "the loop is not counted, and not counted without end";
    EXPECT_TRUE(fs::is_empty(target_));
}

TEST_F(SymlinkFollowTest, LinkInTheTargetIsNotWrittenThrough)  // [SymlinkFollowTests.md#LinkInTheTargetIsNotWrittenThrough]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_directories(root_ / "anderswo");
    fs::create_directory_symlink(root_ / "anderswo", target_ / "raus");
    Outcome outcome;

    run(item(), outcome);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(fs::is_empty(root_ / "anderswo"));
}

TEST_F(SymlinkFollowTest, WithTwoWayOrMoveTheModeFallsBackAndSaysSo)  // [SymlinkFollowTests.md#WithTwoWayOrMoveTheModeFallsBackAndSaysSo]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    SyncItem si = item();
    si.two_way_sync(true);
    Outcome outcome;

    run(si, outcome);

    EXPECT_FALSE(fs::exists(target_ / "raus")) << "writes and deletions must not travel through the link";
    EXPECT_FALSE(fs::exists(outside_ / "b.txt"));
    EXPECT_EQ(logged("symlink_follow_needs_one_way"), 1);
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b") << "the rest of the item is synchronised as in the default mode";
}

TEST_F(SymlinkFollowTest, DirectoryItemStillDoesNotDescend)  // [SymlinkFollowTests.md#DirectoryItemStillDoesNotDescend]
{
    fs::create_directory_symlink("../fremd", source_ / "raus");
    fs::create_symlink("../fremd/f.txt", source_ / "datei.txt");
    Outcome outcome;

    run(item(SyncItemType::Dir), outcome);

    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "datei.txt"}));
    EXPECT_EQ(tu::read_text_file(target_ / "datei.txt"), "f");
}
#endif
