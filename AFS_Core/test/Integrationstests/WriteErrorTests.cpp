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
#include "CopyLog.h"
#include "FileUtil.h"
#include "SyncFile.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <FilesystemUtil.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#ifdef _UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;
using fsu::FileInfoResult;

class WriteTimeError {  // [WriteErrorTests.md#WriteTimeError]
public:
    explicit WriteTimeError(std::errc error) { fsu::set_write_time_error_for_test(std::make_error_code(error)); }
    WriteTimeError(const WriteTimeError&) = delete;
    WriteTimeError& operator=(const WriteTimeError&) = delete;
    ~WriteTimeError() { fsu::set_write_time_error_for_test(std::nullopt); }
};

class RightsError {  // [WriteErrorTests.md#RightsError]
public:
    explicit RightsError(std::errc error) { fsu::set_permissions_error_for_test(std::make_error_code(error)); }
    RightsError(const RightsError&) = delete;
    RightsError& operator=(const RightsError&) = delete;
    ~RightsError() { fsu::set_permissions_error_for_test(std::nullopt); }
};

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class WriteErrorTest : public ::testing::Test {  // [WriteErrorTests.md#WriteErrorTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_write_error_" + std::to_string(TestPaths::process_id()));
    const fs::path source_dir_ = root_ / "quelle";
    const fs::path target_dir_ = root_ / "ziel";

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override
    {
        fsu::set_write_time_error_for_test(std::nullopt);
        fsu::set_permissions_error_for_test(std::nullopt);
        set_copy_hooks_for_test(nullptr, nullptr);
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem file_item(const std::string& name, bool two_way = false) const
    {
        CompareItems ci;
        ci.content(false);
        ci.time(true);
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source((source_dir_ / name).wstring());
        si.target((target_dir_ / name).wstring());
        si.compare_items(ci);
        si.two_way_sync(two_way);
        return si;
    }

    static void run(const SyncItem& si, Outcome& outcome, SyncState* state = nullptr)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
    }

    static bool says(const std::wstring& error, FileInfoResult reason)
    {
        return error.find(fsu::make_error_message(reason)) != std::wstring::npos;
    }

    static bool names(const std::wstring& error, const fs::path& file)
    {
        return error.find(file.wstring()) != std::wstring::npos;
    }
};

} // namespace

TEST_F(WriteErrorTest, FileTimeThatCannotBeSetIsAnErrorOfTheReport)  // [WriteErrorTests.md#FileTimeThatCannotBeSetIsAnErrorOfTheReport]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    Outcome outcome;
    {
        const WriteTimeError refused(std::errc::io_error);
        run(file_item("a.txt"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u) << "the content did arrive";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
    ASSERT_EQ(outcome.report.errors().size(), 1u) << "a target with the wrong time must not count as a clean run";
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::ErrorWriteFileInfo)) << outcome.report.errors().front();
    EXPECT_FALSE(says(outcome.report.errors().front(), FileInfoResult::ErrorReadFileInfo)) << "nothing was read";
    EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / "a.txt")) << outcome.report.errors().front();
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt.bak"));
}

TEST_F(WriteErrorTest, FileTimeErrorNamesTheReasonTheSystemGave)  // [WriteErrorTests.md#FileTimeErrorNamesTheReasonTheSystemGave]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    Outcome outcome;
    {
        const WriteTimeError refused(std::errc::permission_denied);
        run(file_item("a.txt"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::PermissionDenied)) << outcome.report.errors().front();
    EXPECT_FALSE(says(outcome.report.errors().front(), FileInfoResult::ErrorWriteFileInfo)) << "a known reason is not hidden behind the general one";
}

TEST_F(WriteErrorTest, FileTimeThatCannotBeSetOnTheCopyBackNamesTheSourceFile)  // [WriteErrorTests.md#FileTimeThatCannotBeSetOnTheCopyBackNamesTheSourceFile]
{
    tu::write_text_file(source_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "neu", seconds(60));
    Outcome outcome;
    {
        const WriteTimeError refused(std::errc::io_error);
        run(file_item("a.txt", true), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_EQ(tu::read_text_file(source_dir_ / "a.txt"), "neu");
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::ErrorWriteFileInfo)) << outcome.report.errors().front();
    EXPECT_TRUE(names(outcome.report.errors().front(), source_dir_ / "a.txt")) << outcome.report.errors().front();
}

TEST_F(WriteErrorTest, PassAfterwardsSetsTheFileTimeWithoutCopyingAgain)  // [WriteErrorTests.md#PassAfterwardsSetsTheFileTimeWithoutCopyingAgain]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    const SyncItem si = file_item("a.txt");
    Outcome failed;
    {
        const WriteTimeError refused(std::errc::io_error);
        run(si, failed);
    }
    ASSERT_FALSE(failed.report.successful());
    ASSERT_NE(fs::last_write_time(target_dir_ / "a.txt"), fs::last_write_time(source_dir_ / "a.txt"));

    Outcome caught_up;
    run(si, caught_up);

    EXPECT_TRUE(caught_up.report.successful());
    EXPECT_EQ(caught_up.copies, 0u) << "the content is there already, only its time was missing";
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), fs::last_write_time(source_dir_ / "a.txt"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
}

TEST_F(WriteErrorTest, FileTimeThatStaysRefusedIsReportedInEveryPass)  // [WriteErrorTests.md#FileTimeThatStaysRefusedIsReportedInEveryPass]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    const SyncItem si = file_item("a.txt");
    const WriteTimeError refused(std::errc::io_error);
    Outcome first, second;
    run(si, first);

    run(si, second);

    EXPECT_EQ(second.copies, 0u);
    ASSERT_EQ(second.report.errors().size(), 1u) << "the target still bears the wrong time";
    EXPECT_TRUE(says(second.report.errors().front(), FileInfoResult::ErrorWriteFileInfo)) << second.report.errors().front();
    EXPECT_TRUE(names(second.report.errors().front(), target_dir_ / "a.txt"));
}

TEST_F(WriteErrorTest, TwoWayPassAfterwardsSetsTheTargetTimeInsteadOfCopyingBack)  // [WriteErrorTests.md#TwoWayPassAfterwardsSetsTheTargetTimeInsteadOfCopyingBack]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    const fs::file_time_type source_time = fs::last_write_time(source_dir_ / "a.txt");
    const SyncItem si = file_item("a.txt", true);
    SyncState state(si.source(), si.target());
    Outcome failed;
    {
        const WriteTimeError refused(std::errc::io_error);
        run(si, failed, &state);
    }
    ASSERT_EQ(failed.copies, 1u);

    Outcome caught_up;
    run(si, caught_up, &state);

    EXPECT_TRUE(caught_up.report.successful());
    EXPECT_EQ(caught_up.copies, 0u) << "the newer time of the target is no change to carry back";
    EXPECT_EQ(fs::last_write_time(source_dir_ / "a.txt"), source_time);
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), source_time);

    Outcome settled;
    run(si, settled, &state);

    EXPECT_TRUE(settled.report.successful());
    EXPECT_EQ(settled.copies, 0u);
}

TEST_F(WriteErrorTest, TwoWayPassAfterAFailedCopyBackSetsTheSourceTime)  // [WriteErrorTests.md#TwoWayPassAfterAFailedCopyBackSetsTheSourceTime]
{
    tu::write_text_file(source_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "neu", seconds(60));
    const fs::file_time_type target_time = fs::last_write_time(target_dir_ / "a.txt");
    const SyncItem si = file_item("a.txt", true);
    SyncState state(si.source(), si.target());
    Outcome failed;
    {
        const WriteTimeError refused(std::errc::io_error);
        run(si, failed, &state);
    }
    ASSERT_EQ(failed.copies, 1u);

    Outcome caught_up;
    run(si, caught_up, &state);

    EXPECT_TRUE(caught_up.report.successful());
    EXPECT_EQ(caught_up.copies, 0u);
    EXPECT_EQ(fs::last_write_time(source_dir_ / "a.txt"), target_time);
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), target_time);
}

TEST_F(WriteErrorTest, NewerTargetWithTheSameContentGetsTheSourceTime)  // [WriteErrorTests.md#NewerTargetWithTheSameContentGetsTheSourceTime]
{
    tu::write_text_file(source_dir_ / "a.txt", "gleich", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "gleich", seconds(60));
    Outcome outcome;

    run(file_item("a.txt"), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), fs::last_write_time(source_dir_ / "a.txt"));
}

TEST_F(WriteErrorTest, NewerTargetWithOtherContentOfTheSameSizeKeepsItsTime)  // [WriteErrorTests.md#NewerTargetWithOtherContentOfTheSameSizeKeepsItsTime]
{
    tu::write_text_file(source_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "neu", seconds(60));
    const fs::file_time_type target_time = fs::last_write_time(target_dir_ / "a.txt");
    Outcome outcome;

    run(file_item("a.txt"), outcome);

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), target_time) << "an edited target is not the sync's to restamp";
}

TEST_F(WriteErrorTest, PreviewNeitherCountsNorTouchesAFileThatOnlyLacksItsTime)  // [WriteErrorTests.md#PreviewNeitherCountsNorTouchesAFileThatOnlyLacksItsTime]
{
    tu::write_text_file(source_dir_ / "a.txt", "gleich", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "gleich", seconds(60));
    const fs::file_time_type target_time = fs::last_write_time(target_dir_ / "a.txt");
    std::size_t planned = 0;

    SyncFile preview(file_item("a.txt", true), planned, true);
    preview();

    EXPECT_EQ(planned, 0u) << "the run will copy nothing, so the preview must not announce a copy";
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), target_time);
}

TEST_F(WriteErrorTest, CopyWhoseFileTimeIsSetReportsNoError)  // [WriteErrorTests.md#CopyWhoseFileTimeIsSetReportsNoError]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    Outcome outcome;

    run(file_item("a.txt"), outcome);

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), fs::last_write_time(source_dir_ / "a.txt"));
}

TEST_F(WriteErrorTest, RightsThatCannotBeSetAreAnErrorOfTheReport)  // [WriteErrorTests.md#RightsThatCannotBeSetAreAnErrorOfTheReport]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    Outcome outcome;
    {
        const RightsError refused(std::errc::io_error);
        run(file_item("a.txt"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u) << "the content did arrive";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.txt"), fs::last_write_time(source_dir_ / "a.txt")) << "the time is set all the same";
    ASSERT_EQ(outcome.report.errors().size(), 1u) << "a target with other rights than its source must not count as a clean run";
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::CannotSetFileAttributes)) << outcome.report.errors().front();
    EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / "a.txt")) << outcome.report.errors().front();
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt.bak"));
}

TEST_F(WriteErrorTest, RightsErrorNamesTheReasonTheSystemGave)  // [WriteErrorTests.md#RightsErrorNamesTheReasonTheSystemGave]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    Outcome outcome;
    {
        const RightsError refused(std::errc::permission_denied);
        run(file_item("a.txt"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::CannotSetFileAttributes)) << "what failed";
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::PermissionDenied)) << "and why: " << outcome.report.errors().front();
}

TEST_F(WriteErrorTest, FileSystemThatKeepsNoRightsIsNoError)  // [WriteErrorTests.md#FileSystemThatKeepsNoRightsIsNoError]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    Outcome outcome;
    {
        const RightsError unsupported(std::errc::not_supported);
        run(file_item("a.txt"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_TRUE(outcome.report.successful()) << "a backup onto such a drive would otherwise never be clean";
}

TEST_F(WriteErrorTest, RightsThatCannotBeSetOnTheCopyBackNameTheSourceFile)  // [WriteErrorTests.md#RightsThatCannotBeSetOnTheCopyBackNameTheSourceFile]
{
    tu::write_text_file(source_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "neu", seconds(60));
    Outcome outcome;
    {
        const RightsError refused(std::errc::io_error);
        run(file_item("a.txt", true), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_EQ(tu::read_text_file(source_dir_ / "a.txt"), "neu");
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::CannotSetFileAttributes)) << outcome.report.errors().front();
    EXPECT_TRUE(names(outcome.report.errors().front(), source_dir_ / "a.txt")) << outcome.report.errors().front();
}

#ifdef _UNIX
TEST_F(WriteErrorTest, CopyCarriesTheRightsOfTheSourceAndReportsNoError)  // [WriteErrorTests.md#CopyCarriesTheRightsOfTheSourceAndReportsNoError]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    const fs::perms rights = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read;
    fs::permissions(source_dir_ / "a.txt", rights);
    Outcome outcome;

    run(file_item("a.txt"), outcome);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(fs::status(target_dir_ / "a.txt").permissions(), rights);
}

TEST_F(WriteErrorTest, PassAfterwardsSetsTheRightsWithoutCopyingAgain)  // [WriteErrorTests.md#PassAfterwardsSetsTheRightsWithoutCopyingAgain]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu", seconds(60));
    const fs::perms rights = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read;
    fs::permissions(source_dir_ / "a.txt", rights);
    const SyncItem si = file_item("a.txt");
    Outcome failed, still_failing, caught_up, settled;
    {
        const RightsError refused(std::errc::io_error);
        run(si, failed);
        run(si, still_failing);
    }
    ASSERT_FALSE(failed.report.successful());
    EXPECT_FALSE(still_failing.report.successful()) << "the target still has other rights than its source";
    EXPECT_EQ(still_failing.copies, 0u);
    ASSERT_NE(fs::status(target_dir_ / "a.txt").permissions(), rights);

    run(si, caught_up);

    EXPECT_TRUE(caught_up.report.successful());
    EXPECT_EQ(caught_up.copies, 0u) << "the content is there already, only its rights were missing";
    EXPECT_EQ(fs::status(target_dir_ / "a.txt").permissions(), rights);
    fs::permissions(target_dir_ / "a.txt", fs::perms::owner_read | fs::perms::owner_write);

    run(si, settled);

    EXPECT_EQ(fs::status(target_dir_ / "a.txt").permissions(), fs::perms::owner_read | fs::perms::owner_write) << "rights that were set once are the user's to change afterwards";
}

TEST_F(WriteErrorTest, RenameRefusedForAnotherReasonIsAWriteErrorNotAReadError)  // [WriteErrorTests.md#RenameRefusedForAnotherReasonIsAWriteErrorNotAReadError]
{
    const long longest = pathconf(target_dir_.c_str(), _PC_NAME_MAX);
    ASSERT_GT(longest, 8);
    const std::string name = std::string(static_cast<std::size_t>(longest) - 4, 'a') + ".txt";
    tu::write_text_file(source_dir_ / name, "neu", seconds(60));
    tu::write_text_file(target_dir_ / name, "alt", seconds(3600));
    Outcome outcome;

    run(file_item(name), outcome);

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::ErrorWriteFileInfo)) << outcome.report.errors().front();
    EXPECT_FALSE(says(outcome.report.errors().front(), FileInfoResult::ErrorReadFileInfo)) << "the old target was to be moved, not read";
    EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / name));
    EXPECT_EQ(tu::read_text_file(target_dir_ / name), "alt");
}
#endif

TEST_F(WriteErrorTest, FileTimeThePlaceCannotStoreIsAHintAndNoError)  // [WriteErrorTests.md#FileTimeThePlaceCannotStoreIsAHintAndNoError]
{
    for (const char* name : {"a.txt", "b.txt"}) {
        tu::write_text_file(source_dir_ / name, "neu", seconds(60));
        tu::write_text_file(target_dir_ / name, "alt", seconds(3600));
    }
    SyncItem si = file_item("a.txt");
    si.sync_item_type(SyncItemType::Dir);
    si.source(source_dir_.wstring());
    si.target(target_dir_.wstring());
    Outcome outcome;
    {
        const WriteTimeError unsupported(std::errc::operation_not_supported);
        run(si, outcome);
    }

    EXPECT_EQ(outcome.copies, 2u);
    EXPECT_TRUE(outcome.report.successful()) << "was der Ort nicht kann, ist kein Fehler des Laufs";
    ASSERT_EQ(outcome.report.hints().count("file_times_not_supported"), 1u);
    EXPECT_EQ(outcome.report.hints().at("file_times_not_supported").count, 2u) << "ein Hinweis fuer beide Dateien";
    ASSERT_EQ(outcome.report.hint_lines().size(), 1u);
    EXPECT_NE(outcome.report.hint_lines().front().find(L" 2 ("), std::wstring::npos) << outcome.report.hint_lines().front();
}

#ifndef _WIN32
TEST_F(WriteErrorTest, ASpecialFileIsSkippedWithAHintInDirectoryAndTreeItems)  // [WriteErrorTests.md#ASpecialFileIsSkippedWithAHintInDirectoryAndTreeItems]
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    ASSERT_EQ(mkfifo((source_dir_ / "rohr").c_str(), 0600), 0);
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        SyncItem si = file_item("a.txt");
        si.sync_item_type(type);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        Outcome outcome;
        run(si, outcome);

        EXPECT_TRUE(outcome.report.successful()) << "eine FIFO ist kein Fehler: " << (outcome.report.errors().empty() ? L"" : outcome.report.errors().front());
        ASSERT_EQ(outcome.report.hints().count("special_files_skipped"), 1u) << static_cast<int>(type);
        EXPECT_EQ(outcome.report.hints().at("special_files_skipped").first, (source_dir_ / "rohr").wstring());
        EXPECT_FALSE(fs::exists(fs::symlink_status(target_dir_ / "rohr")));
    }
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "a");
}
#endif

TEST_F(WriteErrorTest, ADirectoryWhereTheTargetFileShouldBeIsNamedAndLeftAlone)  // [WriteErrorTests.md#ADirectoryWhereTheTargetFileShouldBeIsNamedAndLeftAlone]
{
    tu::write_text_file(source_dir_ / "name", "datei");
    tu::write_text_file(target_dir_ / "name" / "inhalt.txt", "bleibt");
    Outcome outcome;

    run(file_item("name"), outcome);

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::InvalidFileType)) << outcome.report.errors().front();
    EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / "name"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "name" / "inhalt.txt"), "bleibt");
    EXPECT_FALSE(fs::exists(target_dir_ / "name.bak")) << "das Verzeichnis darf nicht beiseite geschoben werden";
}

TEST_F(WriteErrorTest, ATargetChangedSinceTheComparisonIsNotOverwritten)  // [WriteErrorTests.md#ATargetChangedSinceTheComparisonIsNotOverwritten]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu aus der Quelle", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    set_copy_hooks_for_test([&] { tu::write_text_file(target_dir_ / "a.txt", "gerade im Ziel bearbeitet"); }, nullptr);
    Outcome outcome;

    run(file_item("a.txt"), outcome);
    set_copy_hooks_for_test(nullptr, nullptr);

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "gerade im Ziel bearbeitet") << "was nach dem Vergleich ins Ziel kam, darf nicht ueberschrieben werden";
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::TargetFileChanged)) << outcome.report.errors().front();
    EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / "a.txt"));
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt.bak"));
}

TEST_F(WriteErrorTest, ATargetThatAppearedSinceTheComparisonIsNotOverwritten)  // [WriteErrorTests.md#ATargetChangedSinceTheComparisonIsNotOverwritten]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu aus der Quelle");
    set_copy_hooks_for_test([&] { tu::write_text_file(target_dir_ / "a.txt", "eben erst entstanden"); }, nullptr);
    Outcome outcome;

    run(file_item("a.txt"), outcome);
    set_copy_hooks_for_test(nullptr, nullptr);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "eben erst entstanden");
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_TRUE(says(outcome.report.errors().front(), FileInfoResult::TargetFileChanged));
}

TEST_F(WriteErrorTest, ASourceChangedSinceTheComparisonIsNotTakenForCopied)  // [WriteErrorTests.md#ASourceChangedSinceTheComparisonIsNotTakenForCopied]
{
    tu::write_text_file(source_dir_ / "a.txt", "erster Stand", seconds(60));
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    set_copy_hooks_for_test([&] { tu::write_text_file(source_dir_ / "a.txt", "zweiter, laengerer Stand"); }, nullptr);
    Outcome first, second;

    run(file_item("a.txt"), first);
    set_copy_hooks_for_test(nullptr, nullptr);
    run(file_item("a.txt"), second);

    EXPECT_EQ(first.copies, 0u);
    ASSERT_EQ(first.report.errors().size(), 1u);
    EXPECT_TRUE(says(first.report.errors().front(), FileInfoResult::SourceFileChanged)) << first.report.errors().front();
    EXPECT_TRUE(names(first.report.errors().front(), source_dir_ / "a.txt"));
    EXPECT_EQ(second.copies, 1u) << "der naechste Lauf kopiert den neuen Stand";
    EXPECT_TRUE(second.report.successful());
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "zweiter, laengerer Stand");
}

TEST_F(WriteErrorTest, ACopyOfAnotherSizeOrContentIsNamedAsSuchAndTheOldTargetComesBack)  // [WriteErrorTests.md#ACopyOfAnotherSizeOrContentIsNamedAsSuchAndTheOldTargetComesBack]
{
    for (const FileInfoResult expected : {FileInfoResult::FileSizeMismatch, FileInfoResult::ChecksumMismatch}) {
        tu::write_text_file(source_dir_ / "a.txt", "zehn Bytes", seconds(60));
        tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
        const std::string spoiled = expected == FileInfoResult::FileSizeMismatch ? "kurz" : "ZEHN BYTES";
        set_copy_hooks_for_test(nullptr, [&] { std::ofstream(target_dir_ / "a.txt", std::ios::binary | std::ios::trunc) << spoiled; });
        Outcome outcome;

        run(file_item("a.txt"), outcome);
        set_copy_hooks_for_test(nullptr, nullptr);

        ASSERT_EQ(outcome.report.errors().size(), 1u);
        EXPECT_TRUE(says(outcome.report.errors().front(), expected)) << outcome.report.errors().front();
        EXPECT_TRUE(names(outcome.report.errors().front(), target_dir_ / "a.txt"));
        EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt") << "die verdorbene Kopie weicht dem alten Ziel";
        EXPECT_EQ(outcome.copies, 0u);
    }
}
