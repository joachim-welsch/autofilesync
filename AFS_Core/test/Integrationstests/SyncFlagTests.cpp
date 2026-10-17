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
#include "RecycleBin.h"
#include "ScopedDeleteMode.h"
#include "ScopedRecycleBin.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <chrono>
#include <filesystem>

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

class SyncFlagTest: public ::testing::Test {  // [SyncFlagTests.md#SyncFlagTest]
protected:
    const fs::path source_dir_ = fs::path(TestPaths::scratch_dir()) / "sync_flags" / "source";
    const fs::path target_dir_ = fs::path(TestPaths::scratch_dir()) / "sync_flags" / "target";

    void SetUp() override
    {
        remove_test_dirs();
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override { remove_test_dirs(); }

    void remove_test_dirs() const
    {
        std::error_code ec;
        fs::remove_all(source_dir_.parent_path(), ec);
    }

    SyncItem dir_item() const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Dir);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        return si;
    }

    SyncItem file_item(const std::wstring& filename) const
    {
        SyncItem si = dir_item();
        si.sync_item_type(SyncItemType::File);
        si.source((source_dir_ / filename).wstring());
        si.target((target_dir_ / filename).wstring());
        return si;
    }
};

} // namespace

TEST_F(SyncFlagTest, MoveDeletesTheSourceAfterTheCopy)  // [SyncFlagTests.md#MoveDeletesTheSourceAfterTheCopy]
{
    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "brief.txt", "Inhalt");
    SyncItem si = file_item(L"brief.txt");
    si.del_aftercopy(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_FALSE(fs::exists(source_dir_ / "brief.txt")) << "verschieben heisst: die Quelle ist danach weg";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "brief.txt"), "Inhalt");
}

TEST_F(SyncFlagTest, MoveSendsTheSourceToTheRecycleBinInRecycleBinMode)  // [SyncFlagTests.md#MoveSendsTheSourceToTheRecycleBinInRecycleBinMode]
{
    const tu::ScopedDeleteMode recycle(DeleteMode::RecycleBin);
    const tu::ScopedRecycleBin bin;
    tu::write_text_file(source_dir_ / "brief.txt", "Inhalt");
    SyncItem si = file_item(L"brief.txt");
    si.del_aftercopy(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_FALSE(fs::exists(source_dir_ / "brief.txt"));
    EXPECT_EQ(RecycleBin::instance().list_items().size(), 1u) << "die Quelle landet im Papierkorb, nicht im Nirwana";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "brief.txt"), "Inhalt");
}

TEST_F(SyncFlagTest, MoveLeavesAnUnchangedSourceAlone)  // [SyncFlagTests.md#MoveLeavesAnUnchangedSourceAlone]
{
    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "brief.txt", "gleich");
    tu::write_text_file(target_dir_ / "brief.txt", "gleich");
    SyncItem si = file_item(L"brief.txt");
    si.del_aftercopy(true);

    EXPECT_EQ(tu::run_sync(si), 0u);

    EXPECT_TRUE(fs::exists(source_dir_ / "brief.txt")) << "ohne Kopie darf die Quelle nicht geloescht werden";
}

TEST_F(SyncFlagTest, MoveEmptiesTheSourceDirectoryFileByFile)  // [SyncFlagTests.md#MoveEmptiesTheSourceDirectoryFileByFile]
{
    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.txt", "a");
    tu::write_text_file(source_dir_ / "b.txt", "b");
    SyncItem si = dir_item();
    si.del_aftercopy(true);

    EXPECT_EQ(tu::run_sync(si), 2u);

    EXPECT_TRUE(fs::is_empty(source_dir_)) << "alle Dateien sind verschoben";
    EXPECT_TRUE(fs::exists(source_dir_)) << "das Quellverzeichnis selbst bleibt";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "a");
    EXPECT_EQ(tu::read_text_file(target_dir_ / "b.txt"), "b");
}

TEST_F(SyncFlagTest, NewerTargetIsLeftAloneWithoutCopyWhenTargetNewer)  // [SyncFlagTests.md#NewerTargetIsLeftAloneWithoutCopyWhenTargetNewer]
{
    tu::write_text_file(source_dir_ / "notes.txt", "alt", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "im Ziel bearbeitet");
    SyncItem si = file_item(L"notes.txt");
    si.compare_items(by_time());

    EXPECT_EQ(tu::run_sync(si), 0u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "notes.txt"), "im Ziel bearbeitet");
}

TEST_F(SyncFlagTest, CopyWhenTargetNewerOverwritesTheNewerTarget)  // [SyncFlagTests.md#CopyWhenTargetNewerOverwritesTheNewerTarget]
{
    tu::write_text_file(source_dir_ / "notes.txt", "Quelle gewinnt", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "im Ziel bearbeitet");
    SyncItem si = file_item(L"notes.txt");
    si.compare_items(by_time());
    si.copy_when_target_newer(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "notes.txt"), "Quelle gewinnt");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "Quelle gewinnt");
}

TEST_F(SyncFlagTest, CopyWhenTargetNewerWinsOverTwoWaySync)  // [SyncFlagTests.md#CopyWhenTargetNewerWinsOverTwoWaySync]
{
    tu::write_text_file(source_dir_ / "notes.txt", "Quelle gewinnt", seconds(3600));
    tu::write_text_file(target_dir_ / "notes.txt", "im Ziel bearbeitet");
    SyncItem si = file_item(L"notes.txt");
    si.compare_items(by_time());
    si.copy_when_target_newer(true);
    si.two_way_sync(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "notes.txt"), "Quelle gewinnt");
    EXPECT_EQ(tu::read_text_file(source_dir_ / "notes.txt"), "Quelle gewinnt") << "nichts wird zurueckkopiert";
}

TEST_F(SyncFlagTest, FatherSonKeepsTheOldTargetAsFirstGeneration)  // [SyncFlagTests.md#FatherSonKeepsTheOldTargetAsFirstGeneration]
{
    tu::write_text_file(source_dir_ / "daten.txt", "Sohn");
    tu::write_text_file(target_dir_ / "daten.txt", "Vater");
    SyncItem si = file_item(L"daten.txt");
    si.father_son_principle(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt"), "Sohn");
    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt.bak.1"), "Vater");
    EXPECT_FALSE(fs::exists(target_dir_ / "daten.txt.bak")) << "die Kopiersicherung darf nicht zusaetzlich liegen bleiben";
}

TEST_F(SyncFlagTest, FatherSonNumbersEveryGeneration)  // [SyncFlagTests.md#FatherSonNumbersEveryGeneration]
{
    tu::write_text_file(source_dir_ / "daten.txt", "Sohn");
    tu::write_text_file(target_dir_ / "daten.txt", "Vater");
    SyncItem si = file_item(L"daten.txt");
    si.father_son_principle(true);
    EXPECT_EQ(tu::run_sync(si), 1u);
    tu::write_text_file(source_dir_ / "daten.txt", "Enkel");

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt"), "Enkel");
    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt.bak.1"), "Vater") << "die erste Generation bleibt, wie sie war";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt.bak.2"), "Sohn");
}

TEST_F(SyncFlagTest, FatherSonNeedsNoBackupForANewTarget)  // [SyncFlagTests.md#FatherSonNeedsNoBackupForANewTarget]
{
    tu::write_text_file(source_dir_ / "daten.txt", "Sohn");
    SyncItem si = file_item(L"daten.txt");
    si.father_son_principle(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt"), "Sohn");
    EXPECT_FALSE(fs::exists(target_dir_ / "daten.txt.bak.1"));
}

TEST_F(SyncFlagTest, FatherSonBackupsSurviveDeleteInTarget)  // [SyncFlagTests.md#FatherSonBackupsSurviveDeleteInTarget]
{
    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "daten.txt", "Sohn");
    tu::write_text_file(target_dir_ / "daten.txt", "Vater");
    tu::write_text_file(target_dir_ / "nur-im-ziel.txt", "weg damit");
    SyncItem si = dir_item();
    si.father_son_principle(true);
    si.delete_in_target(true);

    tu::run_sync(si);
    tu::run_sync(si);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt.bak.1"), "Vater") << "die Generation ist kein Ziel-Fremdkoerper";
    EXPECT_FALSE(fs::exists(target_dir_ / "nur-im-ziel.txt")) << "echte Nur-im-Ziel-Dateien werden weiterhin geloescht";
}

TEST_F(SyncFlagTest, FatherSonBackupsAreNotCopiedBackByTwoWaySync)  // [SyncFlagTests.md#FatherSonBackupsAreNotCopiedBackByTwoWaySync]
{
    tu::write_text_file(source_dir_ / "daten.txt", "Sohn");
    tu::write_text_file(target_dir_ / "daten.txt", "Vater", seconds(3600));
    SyncItem si = dir_item();
    si.father_son_principle(true);
    si.two_way_sync(true);

    tu::run_sync(si);
    tu::run_sync(si);

    EXPECT_EQ(tu::read_text_file(target_dir_ / "daten.txt.bak.1"), "Vater");
    EXPECT_FALSE(fs::exists(source_dir_ / "daten.txt.bak.1")) << "Generationen gehoeren ins Ziel, nicht zurueck in die Quelle";
}
