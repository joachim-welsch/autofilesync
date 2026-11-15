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
#if defined(_UNIX) && !defined(_MACOS)
#include <MountedDrives.h>
#include <cstring>
#include <filesystem>
#include "FileDifference.h"
#include "FileUtil.h"
#include "ScopedFstab.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class UnpluggedDriveTest : public ::testing::Test {  // [UnpluggedDriveTests.md#UnpluggedDriveTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("afx_unplugged_" + std::to_string(TestPaths::process_id()));
    const tu::ScopedFstab fstab_{root_ / "fstab"};
    const std::string unplugged_ = "/mnt/afx_unplugged";

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        Synchronize::start();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    std::wstring unplugged(const char* name) const
    {
        return std::wstring(unplugged_.begin(), unplugged_.end()) + L"/" + std::wstring(name, name + std::strlen(name));
    }

    SyncItem file_item(const std::wstring& source, const std::wstring& target) const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source(source);
        si.target(target);
        return si;
    }
};

} // namespace

TEST_F(UnpluggedDriveTest, FileDifferenceTellsAnUnmountedDriveFromAMissingFile)  // [UnpluggedDriveTests.md#FileDifferenceTellsAnUnmountedDriveFromAMissingFile]
{
    fstab_.content("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
    tu::write_text_file(root_ / "da.txt", "da");

    FileDifference target_unplugged((root_ / "da.txt").wstring(), unplugged("a.txt"), CompareItems(), SSHPATH::none, false);
    target_unplugged();
    FileDifference source_unplugged(unplugged("a.txt"), (root_ / "da.txt").wstring(), CompareItems(), SSHPATH::none, false);
    source_unplugged();
    FileDifference target_missing((root_ / "da.txt").wstring(), (root_ / "fehlt.txt").wstring(), CompareItems(), SSHPATH::none, false);
    target_missing();

    EXPECT_EQ(target_unplugged.result(), FileDifference::Result::DestDriveNotMounted);
    EXPECT_EQ(source_unplugged.result(), FileDifference::Result::SourceDriveNotMounted);
    EXPECT_EQ(target_missing.result(), FileDifference::Result::DestNotExists) << "eine fehlende Datei auf eingehaengtem Laufwerk bleibt 'fehlt'";
}

TEST_F(UnpluggedDriveTest, UnpluggedSourceDriveNeverDeletesTheTarget)  // [UnpluggedDriveTests.md#UnpluggedSourceDriveNeverDeletesTheTarget]
{
    fstab_.content("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
    tu::write_text_file(root_ / "ziel.txt", "wertvoll");
    SyncItem si = file_item(unplugged("quelle.txt"), (root_ / "ziel.txt").wstring());
    si.delete_in_target(true);

    EXPECT_EQ(tu::run_sync(si), 0u);

    EXPECT_EQ(tu::read_text_file(root_ / "ziel.txt"), "wertvoll") << "ein abgezogenes Quelllaufwerk ist keine geloeschte Quelle";
}

TEST_F(UnpluggedDriveTest, UnpluggedTargetDriveGetsNoCopy)
{
    fstab_.content("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
    tu::write_text_file(root_ / "quelle.txt", "neu");

    EXPECT_EQ(tu::run_sync(file_item((root_ / "quelle.txt").wstring(), unplugged("ziel.txt"))), 0u);

    EXPECT_FALSE(fs::exists(unplugged_)) << "nichts darf den fehlenden Einhaengepunkt als Verzeichnis anlegen";
}
#endif
