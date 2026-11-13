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
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

constexpr std::size_t MEBIBYTE = 1u << 20;

CompareItems by_time()
{
    CompareItems ci;
    ci.content(false);
    ci.time(true);
    return ci;
}

struct Outcome {
    std::size_t copies;
    SyncReport report;
};

class FileSizeLimit {  // [CopyFailureTests.md#FileSizeLimit]
    rlimit previous_{};
    void (*previous_handler_)(int) = std::signal(SIGXFSZ, SIG_IGN);
public:
    explicit FileSizeLimit(std::size_t bytes)
    {
        getrlimit(RLIMIT_FSIZE, &previous_);
        const rlimit limited{static_cast<rlim_t>(bytes), previous_.rlim_max};
        setrlimit(RLIMIT_FSIZE, &limited);
    }
    FileSizeLimit(const FileSizeLimit&) = delete;
    FileSizeLimit& operator=(const FileSizeLimit&) = delete;
    ~FileSizeLimit()
    {
        setrlimit(RLIMIT_FSIZE, &previous_);
        std::signal(SIGXFSZ, previous_handler_);
    }
};

class RestlessWriter {  // [CopyFailureTests.md#RestlessWriter]
    std::atomic<bool> stop_{false};
    std::thread thread_;
public:
    explicit RestlessWriter(const fs::path& file): thread_([this, file] { keep_writing(file); }) {}
    RestlessWriter(const RestlessWriter&) = delete;
    RestlessWriter& operator=(const RestlessWriter&) = delete;
    ~RestlessWriter()
    {
        stop_ = true;
        thread_.join();
    }
private:
    void keep_writing(const fs::path& file) const
    {
        const int fd = open(file.c_str(), O_WRONLY);
        for (std::uint64_t counter = 1; fd >= 0 && !stop_; ++counter)
            if (pwrite(fd, &counter, sizeof counter, 0) < 0)
                break;
        if (fd >= 0)
            close(fd);
    }
};

class CopyFailureTest : public ::testing::Test {  // [CopyFailureTests.md#CopyFailureTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_copy_failure_" + std::to_string(TestPaths::process_id()));
    const fs::path source_dir_ = root_ / "quelle";
    const fs::path target_dir_ = root_ / "ziel";

    void SetUp() override
    {
        if (geteuid() == 0)
            GTEST_SKIP() << "root ignoriert Dateirechte";
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::permissions(target_dir_, fs::perms::owner_all, fs::perm_options::add, ec);  // [CopyFailureTests.md#unlock-before-removing]
        fs::remove_all(root_, ec);
    }

    SyncItem file_item(const char* name) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source((source_dir_ / name).wstring());
        si.target((target_dir_ / name).wstring());
        si.compare_items(by_time());
        return si;
    }

    SyncItem two_way_dir_item() const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Dir);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        si.two_way_sync(true);
        return si;
    }

    static Outcome run(const SyncItem& si, SyncState* state = nullptr)  // [LocalPermissionTests.md#run]
    {
        Outcome outcome;
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
        return outcome;
    }
};

} // namespace

TEST_F(CopyFailureTest, WriteErrorDuringTheCopyRestoresTheOldTarget)  // [CopyFailureTests.md#WriteErrorDuringTheCopyRestoresTheOldTarget]
{
    tu::write_large_file(source_dir_ / "a.bin", 3 * MEBIBYTE);
    tu::write_text_file(target_dir_ / "a.bin", "alt", seconds(3600));
    const tu::ScopedLog small_log(root_ / "copy_failure.log");  // [CopyFailureTests.md#log-below-the-limit]
    Outcome outcome;
    {
        const FileSizeLimit full_disk(MEBIBYTE);
        outcome = run(file_item("a.bin"));
    }

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful()) << "eine abgebrochene Kopie muss als Fehler gemeldet werden";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.bin"), "alt") << "das alte Ziel muss nach dem Schreibfehler zurueckkommen";
    EXPECT_FALSE(fs::exists(target_dir_ / "a.bin.bak"));
}

TEST_F(CopyFailureTest, NewFileThatCannotBeWrittenLeavesNoHalfFile)  // [CopyFailureTests.md#NewFileThatCannotBeWrittenLeavesNoHalfFile]
{
    tu::write_large_file(source_dir_ / "a.bin", 3 * MEBIBYTE);
    const tu::ScopedLog small_log(root_ / "copy_failure.log");  // [CopyFailureTests.md#log-below-the-limit]
    Outcome outcome;
    {
        const FileSizeLimit full_disk(MEBIBYTE);
        outcome = run(file_item("a.bin"));
    }

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful());
    EXPECT_FALSE(fs::exists(target_dir_ / "a.bin")) << "eine halbe Datei wuerde beim naechsten Lauf als vollstaendig gelten";
}

TEST_F(CopyFailureTest, SourceChangedDuringTheCopyRestoresTheOldTarget)  // [CopyFailureTests.md#SourceChangedDuringTheCopyRestoresTheOldTarget]
{
    tu::write_large_file(source_dir_ / "a.bin", 16 * MEBIBYTE);
    tu::write_text_file(target_dir_ / "a.bin", "alt", seconds(3600));
    Outcome outcome;
    {
        const RestlessWriter writer(source_dir_ / "a.bin");
        outcome = run(file_item("a.bin"));
    }

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful()) << "eine Kopie, die von der Quelle abweicht, muss als Fehler gemeldet werden";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.bin"), "alt");
    EXPECT_FALSE(fs::exists(target_dir_ / "a.bin.bak"));
}

TEST_F(CopyFailureTest, UsersOwnBakFileBesideTheTargetSurvivesTheCopy)  // [CopyFailureTests.md#UsersOwnBakFileBesideTheTargetSurvivesTheCopy]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt.bak", "Sicherung des Benutzers", seconds(7200));

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt.bak")) << "eine fremde Datei neben dem Ziel darf der Kopie nicht zum Opfer fallen";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt.bak"), "Sicherung des Benutzers");
}

TEST_F(CopyFailureTest, UsersOwnBakFileSurvivesTheFirstCopyOfItsNeighbour)  // [CopyFailureTests.md#UsersOwnBakFileBesideTheTargetSurvivesTheCopy]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt.bak", "Sicherung des Benutzers", seconds(7200));

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 1u);
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt.bak")) << "ohne altes Ziel gibt es nichts zu sichern und nichts aufzuraeumen";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt.bak"), "Sicherung des Benutzers");
}

TEST_F(CopyFailureTest, UsersOwnBakFileSurvivesAFailedCopy)  // [CopyFailureTests.md#UsersOwnBakFileBesideTheTargetSurvivesTheCopy]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt.bak", "Sicherung des Benutzers", seconds(7200));
    fs::permissions(source_dir_ / "a.txt", fs::perms::none);

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_FALSE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt.bak"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt.bak"), "Sicherung des Benutzers");
}

TEST_F(CopyFailureTest, FailedDeletionIsReportedAndCaughtUpOnLater)  // [CopyFailureTests.md#FailedDeletionIsReportedAndCaughtUpOnLater]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "gone.txt", "einmal synchronisiert", seconds(3600));
    const SyncItem si = two_way_dir_item();
    SyncState state(si.source(), si.target());
    ASSERT_EQ(tu::run_sync(si, state), 1u);
    fs::remove(source_dir_ / "gone.txt");
    fs::permissions(target_dir_, fs::perms::owner_read | fs::perms::owner_exec);

    const Outcome refused = run(si, &state);

    EXPECT_FALSE(refused.report.successful()) << "eine nicht loeschbare Datei muss als Fehler gemeldet werden";
    EXPECT_TRUE(fs::exists(target_dir_ / "gone.txt"));
    EXPECT_FALSE(fs::exists(source_dir_ / "gone.txt")) << "die Datei darf nicht stattdessen in die Quelle zurueckkopiert werden";
    EXPECT_NE(state.find(L"gone.txt"), nullptr) << "ohne den Eintrag gaelte die Datei beim naechsten Lauf als neu";
    fs::permissions(target_dir_, fs::perms::owner_all);

    const Outcome caught_up = run(si, &state);

    EXPECT_TRUE(caught_up.report.successful());
    EXPECT_FALSE(fs::exists(target_dir_ / "gone.txt"));
    EXPECT_FALSE(fs::exists(source_dir_ / "gone.txt"));
}
#endif
