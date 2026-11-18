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
#include "CopyLog.h"
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "ScopedRecycleBin.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::milliseconds;

class ReadOnly {  // [ReadOnlyFilesystemTests.md#ReadOnly]
public:
    explicit ReadOnly(const fs::path& root) { fsu::set_read_only_filesystem_for_test(root); }
    ReadOnly(const ReadOnly&) = delete;
    ReadOnly& operator=(const ReadOnly&) = delete;
    ~ReadOnly() { fsu::set_read_only_filesystem_for_test(std::nullopt); }
};

class ReadOnlyFilesystemTest : public ::testing::Test {  // [ReadOnlyFilesystemTests.md#ReadOnlyFilesystemTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_read_only_" + std::to_string(TestPaths::process_id()));
    const fs::path source_dir_ = root_ / "quelle";
    const fs::path target_dir_ = root_ / "ziel";
    tu::ScopedRecycleBin bin_;

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Config::instance().reset();
        Synchronize::start();
    }

    void TearDown() override
    {
        fsu::set_read_only_filesystem_for_test(std::nullopt);
        fsu::set_write_time_error_for_test(std::nullopt);
        if (Synchronize::has_instance())
            Synchronize::destroy();
        Config::instance().reset();
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem item() const
    {
        CompareItems ci;
        ci.content(false);
        ci.time(true);
        SyncItem si(L"schreibschutz", SyncItemType::Dir, source_dir_.wstring(), target_dir_.wstring(), L"*");
        si.sync_cycle(0);
        si.compare_items(ci);
        return si;
    }

    void write_three_source_files() const
    {
        for (const char* name : {"a.txt", "b.txt", "c.txt"})
            tu::write_text_file(source_dir_ / name, name);
    }

    static LastRun pass(const SyncItem& si)  // [ReadOnlyFilesystemTests.md#pass]
    {
        Config::instance().reset();
        Config::instance().add(si);
        Synchronize::create(SyncType::Single);
        std::this_thread::sleep_for(milliseconds(500));
        Synchronize::destroy();
        const std::optional<LastRun> run = Config::instance().syncitems().at(0).last_run();
        return run.value_or(LastRun{false, 0, L"kein Lauf", std::nullopt});
    }

    static std::size_t entries_in(const fs::path& dir)
    {
        return static_cast<std::size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
    }

    static bool names(const std::wstring& message, const char* message_id)
    {
        return message.find(translation::translate(message_id)) != std::wstring::npos;
    }
};

TEST_F(ReadOnlyFilesystemTest, AReadOnlyTargetIsOneErrorAndNothingIsTried)  // [ReadOnlyFilesystemTests.md#AReadOnlyTargetIsOneErrorAndNothingIsTried]
{
    write_three_source_files();
    const ReadOnly target(target_dir_);

    const LastRun run = pass(item());

    EXPECT_TRUE(run.reachable);
    EXPECT_EQ(run.errors, 1u);
    EXPECT_EQ(run.first_error, translation::translate("target_read_only") + target_dir_.wstring());
    EXPECT_EQ(entries_in(target_dir_), 0u) << "kein Kopierversuch in ein schreibgeschuetztes Ziel";
}

TEST_F(ReadOnlyFilesystemTest, ATargetThatIsWritableAgainIsSynchronized)  // [ReadOnlyFilesystemTests.md#ATargetThatIsWritableAgainIsSynchronized]
{
    write_three_source_files();
    {
        const ReadOnly target(target_dir_);
        ASSERT_EQ(pass(item()).errors, 1u);
    }

    const LastRun run = pass(item());

    EXPECT_EQ(run.errors, 0u) << util::wstring_to_string(run.first_error);
    EXPECT_EQ(entries_in(target_dir_), 3u);
}

TEST_F(ReadOnlyFilesystemTest, ATwoWayItemWithAReadOnlySourceIsOneErrorAndNothingIsTried)  // [ReadOnlyFilesystemTests.md#ATwoWayItemWithAReadOnlySourceIsOneErrorAndNothingIsTried]
{
    write_three_source_files();
    tu::write_text_file(target_dir_ / "nur-im-ziel.txt", "z");
    SyncItem si = item();
    si.two_way_sync(true);
    const ReadOnly source(source_dir_);

    const LastRun run = pass(si);

    EXPECT_EQ(run.errors, 1u);
    EXPECT_EQ(run.first_error, translation::translate("source_read_only") + source_dir_.wstring());
    EXPECT_EQ(entries_in(target_dir_), 1u);
    EXPECT_EQ(entries_in(source_dir_), 3u);
}

TEST_F(ReadOnlyFilesystemTest, AReadOnlySourceOfAPlainCopyIsNoError)  // [ReadOnlyFilesystemTests.md#AReadOnlySourceOfAPlainCopyIsNoError]
{
    write_three_source_files();
    const ReadOnly source(source_dir_);

    const LastRun run = pass(item());

    EXPECT_EQ(run.errors, 0u) << util::wstring_to_string(run.first_error);
    EXPECT_EQ(entries_in(target_dir_), 3u);
}

TEST_F(ReadOnlyFilesystemTest, AMoveFromAReadOnlySourceCopiesAndSaysOnceThatTheSourceStays)  // [ReadOnlyFilesystemTests.md#AMoveFromAReadOnlySourceCopiesAndSaysOnceThatTheSourceStays]
{
    write_three_source_files();
    SyncItem si = item();
    si.del_aftercopy(true);
    const ReadOnly source(source_dir_);

    const LastRun first = pass(si);
    const LastRun second = pass(si);

    for (const LastRun& run : {first, second}) {
        EXPECT_EQ(run.errors, 1u);
        EXPECT_EQ(run.first_error, translation::translate("source_read_only_not_moved") + source_dir_.wstring());
    }
    EXPECT_EQ(entries_in(target_dir_), 3u);
    EXPECT_EQ(entries_in(source_dir_), 3u) << "kein Loeschversuch in einer schreibgeschuetzten Quelle";
    EXPECT_FALSE(fs::exists(bin_.path())) << "nichts darf in den Papierkorb kopiert worden sein";
}

TEST_F(ReadOnlyFilesystemTest, AMoveIsFinishedOnceTheSourceIsWritable)  // [ReadOnlyFilesystemTests.md#AMoveIsFinishedOnceTheSourceIsWritable]
{
    write_three_source_files();
    SyncItem si = item();
    si.del_aftercopy(true);
    {
        const ReadOnly source(source_dir_);
        ASSERT_EQ(pass(si).errors, 1u);
    }

    const LastRun run = pass(si);

    EXPECT_EQ(run.errors, 0u) << util::wstring_to_string(run.first_error);
    EXPECT_EQ(entries_in(source_dir_), 0u);
    EXPECT_EQ(entries_in(target_dir_), 3u);
}

TEST_F(ReadOnlyFilesystemTest, AWriteRefusedInTheMiddleOfAPassAbandonsTheItemWithOneError)  // [ReadOnlyFilesystemTests.md#AWriteRefusedInTheMiddleOfAPassAbandonsTheItemWithOneError]
{
    write_three_source_files();
    fsu::set_write_time_error_for_test(std::make_error_code(std::errc::read_only_file_system));
    SyncReport report;
    const auto start = std::chrono::steady_clock::now();
    {
        SyncReportGuard guard(&report);
        tu::run_sync(item());
        EXPECT_TRUE(sync_abandoned());
        EXPECT_TRUE(Synchronize::stopping()) << "der Rest des Elements muss wie bei einem Stopp uebersprungen werden";
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(sync_abandoned()) << "das naechste Element laeuft wieder";
    EXPECT_FALSE(Synchronize::stopping());
    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_TRUE(names(report.errors().front(), "file_time_not_settable"));
    EXPECT_TRUE(names(report.errors().front(), "filesystem_read_only"));
    EXPECT_TRUE(names(report.errors().front(), "remaining_files_skipped"));
    EXPECT_EQ(entries_in(target_dir_), 1u) << "nach dem ersten Fehlschlag wird keine weitere Datei versucht";
    EXPECT_LT(elapsed, milliseconds(800));
}

TEST_F(ReadOnlyFilesystemTest, AFileTimeRefusedByAReadOnlyFilesystemIsNotTriedAgain)  // [ReadOnlyFilesystemTests.md#AFileTimeRefusedByAReadOnlyFilesystemIsNotTriedAgain]
{
    tu::write_text_file(target_dir_ / "nur-im-ziel.txt", "z");
    SyncItem si = item();
    si.two_way_sync(true);
    fsu::set_write_time_error_for_test(std::make_error_code(std::errc::read_only_file_system));
    SyncReport report;
    const auto start = std::chrono::steady_clock::now();
    {
        SyncReportGuard guard(&report);
        tu::run_sync(si);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_TRUE(names(report.errors().front(), "filesystem_read_only"));
    EXPECT_TRUE(fs::exists(source_dir_ / "nur-im-ziel.txt")) << "die Kopie zurueck in die Quelle ist der Weg mit den Wiederholungen";
    EXPECT_LT(elapsed, milliseconds(800)) << "zehn Versuche im Abstand von 100 ms waeren eine Sekunde je Datei";
}

TEST_F(ReadOnlyFilesystemTest, ADeletionOnAReadOnlyFilesystemNamesTheReasonAndCopiesNothingIntoTheBin)  // [ReadOnlyFilesystemTests.md#ADeletionOnAReadOnlyFilesystemNamesTheReasonAndCopiesNothingIntoTheBin]
{
    const tu::ScopedDeleteMode mode(DeleteMode::RecycleBin);
    for (const char* name : {"alt1.txt", "alt2.txt"})
        tu::write_text_file(target_dir_ / name, name);
    SyncItem si = item();
    si.delete_in_target(true);
    const ReadOnly target(target_dir_);
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        tu::run_sync(si);
    }

    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_TRUE(names(report.errors().front(), "target_delete_failed"));
    EXPECT_TRUE(names(report.errors().front(), "filesystem_read_only"));
    EXPECT_EQ(entries_in(target_dir_), 2u);
    EXPECT_FALSE(fs::exists(bin_.path())) << "die Datei darf nicht erst in den Papierkorb kopiert werden";
}

TEST(SyncReportTests, AReadOnlyRefusalAbandonsTheReportOnce)  // [ReadOnlyFilesystemTests.md#AReadOnlyRefusalAbandonsTheReportOnce]
{
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        report_sync_refusal(L"verweigert", fsu::FileInfoResult::PermissionDenied);
        EXPECT_FALSE(sync_abandoned()) << "nur der Schreibschutz gilt fuer alle weiteren Dateien";
        report_sync_refusal(L"erste", fsu::FileInfoResult::ReadOnlyFilesystem);
        report_sync_refusal(L"zweite", fsu::FileInfoResult::ReadOnlyFilesystem);
        EXPECT_TRUE(sync_abandoned());
    }
    report_sync_refusal(L"ohne Bericht", fsu::FileInfoResult::ReadOnlyFilesystem);

    ASSERT_EQ(report.errors().size(), 2u);
    EXPECT_EQ(report.errors()[0], L"verweigert");
    EXPECT_EQ(report.errors()[1], L"erste " + translation::translate("remaining_files_skipped"));
    EXPECT_FALSE(sync_abandoned());
}

} // namespace
