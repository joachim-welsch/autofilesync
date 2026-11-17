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
#include "SyncState.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <chrono>
#include <filesystem>
#include <unistd.h>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

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

class LocalPermissionTest : public ::testing::Test {  // [LocalPermissionTests.md#LocalPermissionTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_permissions_" + std::to_string(TestPaths::process_id()));
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
        for (const fs::path& dir : {root_, source_dir_, target_dir_, root_ / "gesperrt"})
            fs::permissions(dir, fs::perms::owner_all, fs::perm_options::add, ec);  // [LocalPermissionTests.md#unlock-before-removing]
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root_, fs::directory_options::skip_permission_denied, ec))
            fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add, ec);
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

    SyncItem dir_item(const fs::path& target) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Dir);
        si.source(source_dir_.wstring());
        si.target(target.wstring());
        si.compare_items(by_time());
        return si;
    }

    static Outcome run(const SyncItem& si)  // [LocalPermissionTests.md#run]
    {
        Outcome outcome;
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si);
        return outcome;
    }

    static void lock(const fs::path& path, fs::perms kept = fs::perms::owner_read | fs::perms::owner_exec)
    {
        fs::permissions(path, kept);
    }
};

} // namespace

TEST_F(LocalPermissionTest, ReadOnlyTargetFileIsStillReplaced)  // [LocalPermissionTests.md#ReadOnlyTargetFileIsStillReplaced]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    lock(target_dir_ / "a.txt", fs::perms::owner_read);

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
}

TEST_F(LocalPermissionTest, UnwritableTargetDirectoryKeepsTheOldTargetAndReportsTheError)  // [LocalPermissionTests.md#UnwritableTargetDirectoryKeepsTheOldTargetAndReportsTheError]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    lock(target_dir_);

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful()) << "eine nicht ersetzbare Zieldatei muss als Fehler gemeldet werden";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt.bak"));
}

TEST_F(LocalPermissionTest, NewFileInAnUnwritableTargetDirectoryIsReported)  // [LocalPermissionTests.md#NewFileInAnUnwritableTargetDirectoryIsReported]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    lock(target_dir_);

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((target_dir_ / "a.txt").wstring()), std::wstring::npos) << "die Meldung muss das Ziel nennen";
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
}

TEST_F(LocalPermissionTest, UnreadableSourceKeepsTheOldTargetAndReportsTheError)  // [LocalPermissionTests.md#UnreadableSourceKeepsTheOldTargetAndReportsTheError]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    lock(source_dir_ / "a.txt", fs::perms::none);

    const Outcome outcome = run(file_item("a.txt"));

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt") << "das alte Ziel muss nach dem Fehlschlag zurueckkommen";
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt.bak"));
}

TEST_F(LocalPermissionTest, UncreatableTargetDirectoryIsReportedByName)  // [LocalPermissionTests.md#UncreatableTargetDirectoryIsReportedByName]
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    fs::create_directories(root_ / "gesperrt");
    lock(root_ / "gesperrt");
    const fs::path target = root_ / "gesperrt" / "ziel";

    const Outcome outcome = run(dir_item(target));

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_FALSE(outcome.report.errors().empty());
    EXPECT_NE(outcome.report.errors().front().find(target.wstring()), std::wstring::npos) << outcome.report.errors().front();
    EXPECT_FALSE(fs::exists(target));
}

TEST_F(LocalPermissionTest, UnreadableSourceDirectoryIsReportedAndCopiesNothing)  // [LocalPermissionTests.md#UnreadableSourceDirectoryIsReportedAndCopiesNothing]
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    lock(source_dir_, fs::perms::none);

    const Outcome outcome = run(dir_item(target_dir_));

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find(source_dir_.wstring()), std::wstring::npos);
    EXPECT_TRUE(fs::is_empty(target_dir_));
}

TEST_F(LocalPermissionTest, FileBehindALockedSourceDirectoryIsNotDeletedInTheTarget)  // [LocalPermissionTests.md#locked-source-is-no-deletion]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "wertvoll", seconds(3600));
    tu::write_text_file(target_dir_ / "a.txt", "wertvoll", seconds(3600));
    lock(source_dir_, fs::perms::none);
    SyncItem si = file_item("a.txt");
    si.delete_in_target(true);

    const Outcome outcome = run(si);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "wertvoll") << "die Quelle ist gesperrt, nicht geloescht";
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((source_dir_ / "a.txt").wstring()), std::wstring::npos) << outcome.report.errors().front();
}

TEST_F(LocalPermissionTest, ListableButLockedSourceDirectoryDeletesNothingInTheTarget)  // [LocalPermissionTests.md#locked-source-is-no-deletion]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        fs::permissions(source_dir_, fs::perms::owner_all);
        tu::write_text_file(source_dir_ / "a.txt", "wertvoll", seconds(3600));
        tu::write_text_file(source_dir_ / "sub" / "b.txt", "wertvoll", seconds(3600));
        tu::write_text_file(target_dir_ / "a.txt", "wertvoll", seconds(3600));
        tu::write_text_file(target_dir_ / "sub" / "b.txt", "wertvoll", seconds(3600));
        lock(source_dir_, fs::perms::owner_read);
        SyncItem si = dir_item(target_dir_);
        si.sync_item_type(type);
        si.delete_in_target(true);

        const Outcome outcome = run(si);

        EXPECT_TRUE(fs::exists(target_dir_ / "a.txt")) << static_cast<int>(type);
        EXPECT_TRUE(fs::exists(target_dir_ / "sub" / "b.txt")) << static_cast<int>(type);
        EXPECT_EQ(outcome.copies, 0u) << static_cast<int>(type);
        EXPECT_FALSE(outcome.report.successful()) << "ein Durchlauf, der die Quelle nicht lesen konnte, ist kein Erfolg: " << static_cast<int>(type);
    }
}

TEST_F(LocalPermissionTest, UnlockedSourceSyncsAgainWithoutLoss)  // [LocalPermissionTests.md#locked-source-is-no-deletion]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "weg.txt", "nur im Ziel", seconds(3600));
    lock(source_dir_, fs::perms::owner_read);
    SyncItem si = dir_item(target_dir_);
    si.delete_in_target(true);
    run(si);
    ASSERT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
    ASSERT_TRUE(fs::exists(target_dir_ / "weg.txt")) << "solange die Quelle gesperrt ist, wird auch Ueberzaehliges nicht geloescht";
    fs::permissions(source_dir_, fs::perms::owner_all);

    const Outcome outcome = run(si);

    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "neu");
    EXPECT_FALSE(fs::exists(target_dir_ / "weg.txt")) << "mit lesbarer Quelle gilt das Loeschen im Ziel wieder";
}

TEST_F(LocalPermissionTest, FileBehindALockedTargetDirectoryIsReportedAndNothingIsCopied)  // [LocalPermissionTests.md#FileBehindALockedTargetDirectoryIsReportedAndNothingIsCopied]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    lock(target_dir_, fs::perms::none);

    const Outcome outcome = run(file_item("a.txt"));

    fs::permissions(target_dir_, fs::perms::owner_all);
    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((target_dir_ / "a.txt").wstring()), std::wstring::npos) << outcome.report.errors().front();
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
}

TEST_F(LocalPermissionTest, TwoWayWithALockedDirectoryDeletesAndCopiesNothing)  // [LocalPermissionTests.md#TwoWayWithALockedDirectoryDeletesAndCopiesNothing]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    for (const bool source_locked : {true, false})
        for (const fs::perms kept : {fs::perms::none, fs::perms::owner_read}) {
            std::error_code ec;
            fs::remove_all(source_dir_, ec);
            fs::remove_all(target_dir_, ec);
            fs::create_directories(target_dir_);
            tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
            tu::write_text_file(source_dir_ / "sub" / "b.txt", "b", seconds(3600));
            SyncItem si = dir_item(target_dir_);
            si.sync_item_type(SyncItemType::Tree);
            si.two_way_sync(true);
            SyncState state(si.source(), si.target());
            ASSERT_EQ(tu::run_sync(si, state), 2u);
            const fs::path locked = (source_locked ? source_dir_ : target_dir_) / "sub";
            lock(locked, kept);
            Outcome outcome;
            {
                SyncReportGuard guard(&outcome.report);
                outcome.copies = tu::run_sync(si, state);
            }
            fs::permissions(locked, fs::perms::owner_all);
            const std::string which = std::string(source_locked ? "Quelle" : "Ziel") + (kept == fs::perms::none ? " ohne Rechte" : " nur lesbar");

            EXPECT_EQ(outcome.copies, 0u) << which;
            EXPECT_FALSE(outcome.report.successful()) << which;
            EXPECT_EQ(tu::read_text_file(source_dir_ / "sub" / "b.txt"), "b") << which;
            EXPECT_EQ(tu::read_text_file(target_dir_ / "sub" / "b.txt"), "b") << which;
            EXPECT_NE(state.find(L"sub/b.txt"), nullptr) << "der Stand darf die gesperrte Datei nicht vergessen: " << which;
            EXPECT_EQ(tu::run_sync(si, state), 0u) << "nach dem Entsperren ist alles wie zuvor: " << which;
            EXPECT_TRUE(fs::exists(source_dir_ / "sub" / "b.txt") && fs::exists(target_dir_ / "sub" / "b.txt")) << which;
        }
}

TEST_F(LocalPermissionTest, TwoWayFileItemWithALockedSideDeletesAndCopiesNothing)  // [LocalPermissionTests.md#TwoWayWithALockedDirectoryDeletesAndCopiesNothing]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    for (const bool source_locked : {true, false}) {
        tu::write_text_file(source_dir_ / "a.txt", "a", seconds(3600));
        fs::remove(target_dir_ / "a.txt");
        SyncItem si = file_item("a.txt");
        si.two_way_sync(true);
        SyncState state(si.source(), si.target());
        ASSERT_EQ(tu::run_sync(si, state), 1u);
        const fs::path locked = source_locked ? source_dir_ : target_dir_;
        lock(locked, fs::perms::none);
        Outcome outcome;
        {
            SyncReportGuard guard(&outcome.report);
            outcome.copies = tu::run_sync(si, state);
        }
        fs::permissions(locked, fs::perms::owner_all);

        EXPECT_EQ(outcome.copies, 0u) << source_locked;
        EXPECT_EQ(outcome.report.errors().size(), 1u) << source_locked;
        EXPECT_TRUE(fs::exists(source_dir_ / "a.txt") && fs::exists(target_dir_ / "a.txt")) << source_locked;
        EXPECT_NE(state.find(L"a.txt"), nullptr) << source_locked;
    }
}
#endif
