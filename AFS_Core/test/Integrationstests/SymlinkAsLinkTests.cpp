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
#include "SyncRunner.h"
#include "SyncState.h"
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

class LinkError {  // [SymlinkAsLinkTests.md#LinkError]
public:
    explicit LinkError(std::errc error) { symlink_util::set_link_error_for_test(std::make_error_code(error)); }
    LinkError(const LinkError&) = delete;
    LinkError& operator=(const LinkError&) = delete;
    ~LinkError() { symlink_util::set_link_error_for_test(std::nullopt); }
};

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class SymlinkAsLinkTest : public ::testing::Test {  // [SymlinkAsLinkTests.md#SymlinkAsLinkTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("afs_aslink_" + std::to_string(TestPaths::process_id()));
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";
    const tu::ScopedLog log_{fs::temp_directory_path() / ("autofilesync_aslink_" + std::to_string(TestPaths::process_id()) + ".log"), LogLevel::Info};

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_ / "echt");
        fs::create_directories(target_);
        tu::write_text_file(source_ / "echt" / "a.txt", "a");
        tu::write_text_file(source_ / "b.txt", "b");
        Synchronize::start();
    }

    void TearDown() override
    {
        symlink_util::set_link_error_for_test(std::nullopt);
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
        si.symlink_mode(SymlinkMode::AsLink);
        return si;
    }

    static void run(const SyncItem& si, Outcome& outcome, SyncState* state = nullptr)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
    }

    SyncItem two_way_item(SyncItemType type = SyncItemType::Tree) const
    {
        SyncItem si = item(type);
        si.two_way_sync(true);
        return si;
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

TEST_F(SymlinkAsLinkTest, TreeSyncCarriesFileAndDirectoryLinksOverAsLinks)  // [SymlinkAsLinkTests.md#TreeSyncCarriesFileAndDirectoryLinksOverAsLinks]
{
    fs::create_symlink("b.txt", source_ / "datei.txt");
    fs::create_directory_symlink("echt", source_ / "ordner");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(outcome.copies, 4u) << "two files and two links";
    ASSERT_TRUE(fs::is_symlink(target_ / "datei.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "datei.txt"), fs::path("b.txt"));
    ASSERT_TRUE(fs::is_symlink(target_ / "ordner")) << "the directory is not copied a second time under the link's name";
    EXPECT_EQ(fs::read_symlink(target_ / "ordner"), fs::path("echt"));
    EXPECT_EQ(tu::read_text_file(target_ / "ordner" / "a.txt"), "a") << "the link leads to the copy";
    EXPECT_EQ(tu::read_text_file(target_ / "datei.txt"), "b");
}

TEST_F(SymlinkAsLinkTest, AbsoluteLinkIntoTheTreeBecomesRelative)  // [SymlinkAsLinkTests.md#AbsoluteLinkIntoTheTreeBecomesRelative]
{
    fs::create_symlink(source_ / "b.txt", source_ / "oben.txt");
    fs::create_symlink(source_ / "b.txt", source_ / "echt" / "unten.txt");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_EQ(fs::read_symlink(target_ / "oben.txt"), fs::path("b.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "echt" / "unten.txt"), fs::path("../b.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "unten.txt"), "b") << "it leads to the copy, not back into the source";
    EXPECT_EQ(fs::read_symlink(source_ / "oben.txt"), source_ / "b.txt") << "the source link is not rewritten";
}

TEST_F(SymlinkAsLinkTest, LinksLeavingTheTreeAreCopiedAsTheyAreAndNamedOnce)  // [SymlinkAsLinkTests.md#LinksLeavingTheTreeAreCopiedAsTheyAreAndNamedOnce]
{
    tu::write_text_file(root_ / "fremd.txt", "fremd");
    fs::create_symlink(root_ / "fremd.txt", source_ / "absolut.txt");
    fs::create_symlink("../fremd.txt", source_ / "relativ.txt");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(fs::read_symlink(target_ / "absolut.txt"), root_ / "fremd.txt");
    EXPECT_EQ(fs::read_symlink(target_ / "relativ.txt"), fs::path("../fremd.txt"));
    EXPECT_EQ(logged("links_leave_tree"), 1) << "one line for both links";
}

TEST_F(SymlinkAsLinkTest, DanglingLinkIsCarriedOverAsALink)  // [SymlinkAsLinkTests.md#DanglingLinkIsCarriedOverAsALink]
{
    fs::create_symlink("weg.txt", source_ / "tot.txt");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    ASSERT_TRUE(fs::is_symlink(target_ / "tot.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "tot.txt"), fs::path("weg.txt"));
    EXPECT_FALSE(fs::exists(target_ / "weg.txt"));
}

TEST_F(SymlinkAsLinkTest, SecondPassFindsNothingToDoAndAChangedLinkIsUpdated)  // [SymlinkAsLinkTests.md#SecondPassFindsNothingToDoAndAChangedLinkIsUpdated]
{
    fs::create_symlink(source_ / "b.txt", source_ / "zeiger.txt");
    Outcome first, second, third;
    run(item(), first);

    run(item(), second);

    EXPECT_EQ(second.copies, 0u) << "the rewritten text in the target equals the absolute one in the source";
    fs::remove(source_ / "zeiger.txt");
    fs::create_symlink("echt/a.txt", source_ / "zeiger.txt");

    run(item(), third);

    EXPECT_EQ(third.copies, 1u);
    EXPECT_EQ(fs::read_symlink(target_ / "zeiger.txt"), fs::path("echt/a.txt"));
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt", "zeiger.txt"})) << "no .bak is left";
}

TEST_F(SymlinkAsLinkTest, FileInTheTargetIsReplacedByTheLink)  // [SymlinkAsLinkTests.md#FileInTheTargetIsReplacedByTheLink]
{
    fs::create_symlink("b.txt", source_ / "zeiger.txt");
    tu::write_text_file(target_ / "zeiger.txt", "alte Datei");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_TRUE(fs::is_symlink(target_ / "zeiger.txt"));
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt", "zeiger.txt"}));
}

TEST_F(SymlinkAsLinkTest, LinkThatCannotBeCreatedPutsTheOldTargetBackAndIsAnError)  // [SymlinkAsLinkTests.md#LinkThatCannotBeCreatedPutsTheOldTargetBackAndIsAnError]
{
    fs::create_symlink("b.txt", source_ / "zeiger.txt");
    tu::write_text_file(target_ / "zeiger.txt", "alte Datei");
    Outcome outcome;
    {
        const LinkError refused(std::errc::io_error);
        run(item(), outcome);
    }

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((target_ / "zeiger.txt").wstring()), std::wstring::npos) << "the error names the link";
    EXPECT_FALSE(fs::is_symlink(target_ / "zeiger.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "zeiger.txt"), "alte Datei");
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt", "zeiger.txt"}));
}

TEST_F(SymlinkAsLinkTest, TargetWithoutLinksSkipsThemWithOneWarningAndNoError)  // [SymlinkAsLinkTests.md#TargetWithoutLinksSkipsThemWithOneWarningAndNoError]
{
    fs::create_symlink("b.txt", source_ / "eins.txt");
    fs::create_symlink("b.txt", source_ / "zwei.txt");
    tu::write_text_file(target_ / "zwei.txt", "alte Datei");
    Outcome outcome;
    {
        const LinkError no_links(std::errc::operation_not_permitted);
        run(item(), outcome);
    }

    EXPECT_TRUE(outcome.report.successful()) << "a drive without links is no failure of the run";
    EXPECT_EQ(outcome.copies, 2u) << "only the two files";
    EXPECT_EQ(logged("links_not_supported_skipped"), 1) << "one line for both links";
    EXPECT_FALSE(fs::exists(target_ / "eins.txt")) << "the content is not copied in the link's place";
    EXPECT_EQ(tu::read_text_file(target_ / "zwei.txt"), "alte Datei");
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt", "zwei.txt"}));
}

TEST_F(SymlinkAsLinkTest, DirectoryInTheTargetIsNeverRemovedForALink)  // [SymlinkAsLinkTests.md#DirectoryInTheTargetIsNeverRemovedForALink]
{
    fs::create_directory_symlink("echt", source_ / "ordner");
    tu::write_text_file(target_ / "ordner" / "wichtig.txt", "bleibt");
    Outcome outcome;

    run(item(), outcome);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((target_ / "ordner").wstring()), std::wstring::npos);
    EXPECT_FALSE(fs::is_symlink(target_ / "ordner"));
    EXPECT_EQ(tu::read_text_file(target_ / "ordner" / "wichtig.txt"), "bleibt");
}

TEST_F(SymlinkAsLinkTest, GenerationPrincipleKeepsWhatTheLinkReplaces)  // [SymlinkAsLinkTests.md#GenerationPrincipleKeepsWhatTheLinkReplaces]
{
    fs::create_symlink("b.txt", source_ / "zeiger.txt");
    tu::write_text_file(target_ / "zeiger.txt", "alte Datei");
    SyncItem si = item();
    si.father_son_principle(true);
    Outcome outcome;

    run(si, outcome);

    EXPECT_TRUE(fs::is_symlink(target_ / "zeiger.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "zeiger.txt.bak.1"), "alte Datei");
}

TEST_F(SymlinkAsLinkTest, MoveRemovesTheSourceLinkOnlyAfterTheTargetLinkExists)  // [SymlinkAsLinkTests.md#MoveRemovesTheSourceLinkOnlyAfterTheTargetLinkExists]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);  // [SymlinkAsLinkTests.md#MoveRemovesTheSourceLinkOnlyAfterTheTargetLinkExists]
    fs::create_symlink(root_ / "fremd.txt", source_ / "zeiger.txt");
    SyncItem si = item();
    si.del_aftercopy(true);
    Outcome refused, moved;
    {
        const LinkError no_links(std::errc::io_error);
        run(si, refused);
    }
    EXPECT_TRUE(fs::is_symlink(source_ / "zeiger.txt")) << "no link in the target, so the source keeps its own";

    run(si, moved);

    EXPECT_TRUE(fs::is_symlink(target_ / "zeiger.txt"));
    EXPECT_FALSE(fs::is_symlink(source_ / "zeiger.txt"));
}

TEST_F(SymlinkAsLinkTest, DirSyncCarriesTheLinksOfItsLevelOver)  // [SymlinkAsLinkTests.md#DirSyncCarriesTheLinksOfItsLevelOver]
{
    fs::create_symlink("b.txt", source_ / "datei.txt");
    fs::create_directory_symlink("echt", source_ / "ordner");
    Outcome outcome;

    run(item(SyncItemType::Dir), outcome);

    EXPECT_EQ(outcome.copies, 3u) << "one file and two links";
    EXPECT_EQ(fs::read_symlink(target_ / "datei.txt"), fs::path("b.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "ordner"), fs::path("echt")) << "a directory link is a link like any other";
    EXPECT_FALSE(fs::exists(target_ / "echt")) << "a directory item does not descend";
}

TEST_F(SymlinkAsLinkTest, PreviewCountsLinksAndCreatesNone)  // [SymlinkAsLinkTests.md#PreviewCountsLinksAndCreatesNone]
{
    fs::create_symlink("b.txt", source_ / "datei.txt");
    std::size_t planned = 0;

    SyncTree preview(item(), planned, true);
    preview();

    EXPECT_EQ(planned, 3u);
    EXPECT_TRUE(fs::is_empty(target_));
}

TEST_F(SymlinkAsLinkTest, ExcludedLinkIsNotCarriedOver)  // [SymlinkAsLinkTests.md#ExcludedLinkIsNotCarriedOver]
{
    fs::create_symlink("b.txt", source_ / "datei.tmp");
    fs::create_directory_symlink("echt", source_ / "ordner");
    SyncItem si = item();
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    exclusions.push_back(L"ordner");
    si.exclusions(exclusions);
    Outcome outcome;

    run(si, outcome);

    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt"}));
}

TEST_F(SymlinkAsLinkTest, TargetDirectoryThatIsALinkIsNotWrittenThrough)  // [SymlinkAsLinkTests.md#TargetDirectoryThatIsALinkIsNotWrittenThrough]
{
    fs::create_directories(root_ / "anderswo");
    fs::create_directory_symlink(root_ / "anderswo", target_ / "echt");
    Outcome kept, replaced;

    run(item(), kept);

    ASSERT_EQ(kept.report.errors().size(), 1u) << "the files of the directory did not arrive";
    EXPECT_TRUE(fs::is_empty(root_ / "anderswo")) << "nothing may land where the link points";
    EXPECT_TRUE(fs::is_symlink(target_ / "echt"));
    SyncItem si = item();
    si.delete_in_target(true);

    run(si, replaced);

    EXPECT_TRUE(replaced.report.successful());
    EXPECT_FALSE(fs::is_symlink(target_ / "echt")) << "with deletions allowed the link gives way to the directory";
    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "a.txt"), "a");
    EXPECT_TRUE(fs::is_empty(root_ / "anderswo"));
}

TEST_F(SymlinkAsLinkTest, DeleteInTargetRemovesLinksTheSourceNoLongerHas)  // [SymlinkAsLinkTests.md#DeleteInTargetRemovesLinksTheSourceNoLongerHas]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    fs::create_directories(target_ / "echt");
    tu::write_text_file(target_ / "b.txt", "b");
    fs::create_symlink("b.txt", target_ / "datei.txt");
    fs::create_directory_symlink("echt", target_ / "ordner");
    fs::create_symlink("weg.txt", target_ / "tot.txt");
    Outcome kept, removed;

    run(item(), kept);

    EXPECT_TRUE(fs::is_symlink(target_ / "datei.txt")) << "without the option nothing in the target is deleted";
    SyncItem si = item();
    si.delete_in_target(true);

    run(si, removed);

    EXPECT_TRUE(removed.report.successful());
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt"})) << "the links go, also the one to a directory and the dead one";
    EXPECT_EQ(tu::read_text_file(target_ / "echt" / "a.txt"), "a") << "what a removed link pointed to is not touched";
}

TEST_F(SymlinkAsLinkTest, TwoWaySyncCarriesANewTargetLinkToTheSource)  // [SymlinkAsLinkTests.md#TwoWaySyncCarriesANewTargetLinkToTheSource]
{
    const SyncItem si = two_way_item();
    SyncState state(si.source(), si.target());
    Outcome first, second, third;
    run(si, first, &state);
    fs::create_symlink(target_ / "b.txt", target_ / "neu.txt");
    fs::create_directory_symlink("echt", target_ / "ordner");

    run(si, second, &state);

    EXPECT_TRUE(second.report.successful());
    EXPECT_EQ(fs::read_symlink(source_ / "neu.txt"), fs::path("b.txt")) << "relative on the way back as well";
    EXPECT_EQ(fs::read_symlink(source_ / "ordner"), fs::path("echt"));
    EXPECT_FALSE(fs::is_symlink(source_ / "echt"));

    run(si, third, &state);

    EXPECT_EQ(third.copies, 0u) << "the pair has settled";
    ASSERT_NE(state.find(L"neu.txt"), nullptr);
    EXPECT_EQ(state.find(L"neu.txt")->link, std::optional<std::wstring>(L"b.txt"));
}

TEST_F(SymlinkAsLinkTest, TwoWaySyncDeletesTheLinkOnTheOtherSide)  // [SymlinkAsLinkTests.md#TwoWaySyncDeletesTheLinkOnTheOtherSide]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    fs::create_symlink("b.txt", source_ / "eins.txt");
    fs::create_symlink("b.txt", source_ / "zwei.txt");
    const SyncItem si = two_way_item();
    SyncState state(si.source(), si.target());
    Outcome first, second;
    run(si, first, &state);
    ASSERT_TRUE(fs::is_symlink(target_ / "eins.txt"));
    fs::remove(source_ / "eins.txt");
    fs::remove(target_ / "zwei.txt");

    run(si, second, &state);

    EXPECT_TRUE(second.report.successful());
    EXPECT_FALSE(fs::is_symlink(target_ / "eins.txt")) << "deleted in the source, so it goes in the target";
    EXPECT_FALSE(fs::is_symlink(source_ / "zwei.txt")) << "deleted in the target, so it goes in the source";
    EXPECT_EQ(tu::read_text_file(source_ / "b.txt"), "b");
    EXPECT_EQ(state.find(L"eins.txt"), nullptr);
}

TEST_F(SymlinkAsLinkTest, TwoWaySyncWithoutStateBringsAMissingLinkBackInsteadOfDeleting)  // [SymlinkAsLinkTests.md#TwoWaySyncWithoutStateBringsAMissingLinkBackInsteadOfDeleting]
{
    fs::create_symlink("b.txt", source_ / "eins.txt");
    Outcome outcome;

    run(two_way_item(), outcome);

    EXPECT_TRUE(fs::is_symlink(source_ / "eins.txt"));
    EXPECT_TRUE(fs::is_symlink(target_ / "eins.txt")) << "nothing says the link was ever in the target";
}

TEST_F(SymlinkAsLinkTest, TwoWaySyncCarriesTheChangedTextToTheSideThatDidNotChange)  // [SymlinkAsLinkTests.md#TwoWaySyncCarriesTheChangedTextToTheSideThatDidNotChange]
{
    fs::create_symlink("b.txt", source_ / "eins.txt");
    fs::create_symlink("b.txt", source_ / "zwei.txt");
    const SyncItem si = two_way_item();
    SyncState state(si.source(), si.target());
    Outcome first, second;
    run(si, first, &state);
    fs::remove(source_ / "eins.txt");
    fs::create_symlink("echt/a.txt", source_ / "eins.txt");
    fs::remove(target_ / "zwei.txt");
    fs::create_symlink("echt/a.txt", target_ / "zwei.txt");

    run(si, second, &state);

    EXPECT_TRUE(second.report.successful());
    EXPECT_EQ(fs::read_symlink(target_ / "eins.txt"), fs::path("echt/a.txt"));
    EXPECT_EQ(fs::read_symlink(source_ / "zwei.txt"), fs::path("echt/a.txt"));
    EXPECT_EQ(names_in(source_), (std::set<std::string>{"b.txt", "echt", "eins.txt", "zwei.txt"})) << "nothing parked is left";
}

TEST_F(SymlinkAsLinkTest, LinkChangedOnBothSidesKeepsBothAsAConflictCopy)  // [SymlinkAsLinkTests.md#LinkChangedOnBothSidesKeepsBothAsAConflictCopy]
{
    fs::create_symlink("b.txt", source_ / "zeiger");
    const SyncItem si = two_way_item();
    SyncState state(si.source(), si.target());
    Outcome first, second, third;
    run(si, first, &state);
    fs::remove(source_ / "zeiger");
    fs::create_symlink("echt/a.txt", source_ / "zeiger");
    fs::remove(target_ / "zeiger");
    fs::create_symlink("echt", target_ / "zeiger");

    run(si, second, &state);
    run(si, third, &state);

    EXPECT_EQ(fs::read_symlink(source_ / "zeiger"), fs::path("echt/a.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "zeiger"), fs::path("echt/a.txt")) << "the source's link wins the name";
    ASSERT_EQ(names_in(target_).size(), 4u) << "b.txt, echt, zeiger and the conflict copy";
    EXPECT_EQ(names_in(source_), names_in(target_)) << "the conflict copy travels to the source with the next pass";
    for (const std::string& name : names_in(target_))
        if (name.starts_with("zeiger ("))
            EXPECT_EQ(fs::read_symlink(target_ / name), fs::path("echt")) << "the target's link is kept under the conflict name";
}

TEST_F(SymlinkAsLinkTest, LinkChangedOnBothSidesWithoutConflictCopiesIsLeftAndReported)  // [SymlinkAsLinkTests.md#LinkChangedOnBothSidesWithoutConflictCopiesIsLeftAndReported]
{
    fs::create_symlink("b.txt", source_ / "streit");
    fs::create_symlink("echt", target_ / "streit");
    SyncItem si = two_way_item();
    si.conflict_copies(false);
    Outcome outcome;

    run(si, outcome);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_EQ(fs::read_symlink(source_ / "streit"), fs::path("b.txt"));
    EXPECT_EQ(fs::read_symlink(target_ / "streit"), fs::path("echt"));
}

TEST_F(SymlinkAsLinkTest, FileIsNotWrittenThroughALinkInTheTarget)  // [SymlinkAsLinkTests.md#FileIsNotWrittenThroughALinkInTheTarget]
{
    tu::write_text_file(root_ / "anderswo.txt", "fremd");
    fs::create_symlink(root_ / "anderswo.txt", target_ / "b.txt");
    Outcome kept, replaced;

    run(item(), kept);

    ASSERT_EQ(kept.report.errors().size(), 1u);
    EXPECT_EQ(tu::read_text_file(root_ / "anderswo.txt"), "fremd") << "the copy must not land where the link points";
    EXPECT_TRUE(fs::is_symlink(target_ / "b.txt"));
    SyncItem si = item();
    si.delete_in_target(true);

    run(si, replaced);

    EXPECT_TRUE(replaced.report.successful());
    EXPECT_FALSE(fs::is_symlink(target_ / "b.txt"));
    EXPECT_EQ(tu::read_text_file(target_ / "b.txt"), "b");
    EXPECT_EQ(tu::read_text_file(root_ / "anderswo.txt"), "fremd");
}

TEST_F(SymlinkAsLinkTest, DefaultModeStillTakesATargetOnlyFileLinkForAFile)  // [SymlinkAsLinkTests.md#DefaultModeStillTakesATargetOnlyFileLinkForAFile]
{
    tu::write_text_file(root_ / "anderswo.txt", "fremd");
    fs::create_symlink(root_ / "anderswo.txt", target_ / "nur-im-ziel.txt");
    SyncItem si = two_way_item();
    si.symlink_mode(SymlinkMode::FileContent);
    Outcome outcome;

    run(si, outcome);

    EXPECT_FALSE(fs::is_symlink(source_ / "nur-im-ziel.txt")) << "the default mode copies content, as before";
    EXPECT_EQ(tu::read_text_file(source_ / "nur-im-ziel.txt"), "fremd");
}

#ifdef __linux__
TEST_F(SymlinkAsLinkTest, SystemLinksAreSkippedAndNamedOnce)  // [SymlinkAsLinkTests.md#SystemLinksAreSkippedAndNamedOnce]
{
    fs::create_symlink("/dev/null", source_ / "geraet");
    fs::create_symlink("/proc/version", source_ / "version");
    Outcome outcome;

    run(item(), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(names_in(target_), (std::set<std::string>{"b.txt", "echt"}));
    EXPECT_EQ(logged("system_links_skipped"), 1);
}
#endif

TEST_F(SymlinkAsLinkTest, SystemLinkIsSkippedInTheDefaultModeAsWell)  // [SymlinkAsLinkTests.md#SystemLinksAreSkippedAndNamedOnce]
{
#ifdef __linux__
    fs::create_symlink("/proc/version", source_ / "version");
    SyncItem si = item();
    si.symlink_mode(SymlinkMode::FileContent);
    Outcome outcome;

    run(si, outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_FALSE(fs::exists(target_ / "version")) << "a file of the kernel is nothing to back up";
#endif
}
#endif
