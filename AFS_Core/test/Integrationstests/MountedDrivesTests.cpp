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
#include "MountedDrivesTests.h"
#include <MountedDrives.h>
#include <filesystem>
#include <Logging.h>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <cstring>
#include "FileDifference.h"
#include "FileUtil.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"

using namespace AFS_Core;

using namespace MountedDrivesTests;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

TEST(MountedDrivesTests, /*DISABLED_*/testGetMountPoint)
{
    fs::path mount_point = get_mount_point(TESTMOUNTFN);
	EXPECT_EQ(mount_point, TESTMOUNT2);
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckDriveMounted)
{
    auto& md = MountedDrives::instance();
#ifdef _UNIX
    EXPECT_TRUE(md.drive_mounted(TESTMOUNT));
#else
	EXPECT_TRUE(md.drive_mounted(TESTMOUNT, L"winuser"));
#endif
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckTimeNotElapsed)
{
    auto& md = MountedDrives::instance();
    md.set_test_time(1s);
    EXPECT_FALSE(md.time_elapsed_test());
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckTimeElapsed)
{
    auto& md = MountedDrives::instance();
    md.set_test_time(-11s);
    EXPECT_TRUE(md.time_elapsed_test());
}

TEST(MountedDrivesTests, /*DISABLED_*/ConcurrentDriveMountedCallsDoNotCrash)  // [MountedDrivesTests.md#concurrent-singleton]
{
    auto& md = MountedDrives::instance();
    std::atomic<bool> stop{false};

    std::thread invalidator([&] {
        while (!stop.load())
            md.set_test_time(-11s);
    });

    std::vector<std::thread> checkers;
    for (int i = 0; i < 4; ++i) {
        checkers.emplace_back([&] {
            while (!stop.load()) {
#ifdef _UNIX
                md.drive_mounted(TESTMOUNT);
#else
                md.drive_mounted(TESTMOUNT, L"winuser");
#endif
            }
        });
    }

    std::this_thread::sleep_for(300ms);
    stop = true;
    invalidator.join();
    for (auto& t : checkers)
        t.join();

    SUCCEED();
}

#if defined(_UNIX) && !defined(_MACOS)
TEST(MountedDrivesTests, FstabMountpointDecidesMountedState)  // [MountedDrivesTests.md#FstabMountpointDecidesMountedState]
{
    namespace fs = std::filesystem;
    const fs::path fake_fstab = fs::temp_directory_path() / ("afx_fstab_" + std::to_string(getpid()));
    {
        std::ofstream out(fake_fstab);
        out << "UUID=0000-0000 /mnt/afx_testdrive ext4 defaults 0 2\n";
        out << "proc /proc proc defaults 0 0\n";
    }
    MountedDrives::instance().set_fstab_path_for_test(fake_fstab.string());
    EXPECT_FALSE(MountedDrives::instance().drive_mounted(L"/mnt/afx_testdrive/some/file.txt"));  // [MountedDrivesTests.md#drive-mounted-cases]
    EXPECT_TRUE(MountedDrives::instance().drive_mounted(L"/proc/self/status"));
    EXPECT_TRUE(MountedDrives::instance().drive_mounted(fs::temp_directory_path()));
    MountedDrives::instance().set_fstab_path_for_test("/etc/fstab");
    fs::remove(fake_fstab);
}
#endif
#if defined(_UNIX) && !defined(_MACOS)
namespace {

class FakeFstabTest : public ::testing::Test {  // [MountedDrivesTests.md#FakeFstabTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("afx_fstab_" + std::to_string(TestPaths::process_id()));
    const fs::path fstab_ = root_ / "fstab";
    const std::string unplugged_ = "/mnt/afx_unplugged";

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        Synchronize::start();
    }

    void TearDown() override
    {
        MountedDrives::instance().set_fstab_path_for_test("/etc/fstab");
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void fstab(const std::string& content) const
    {
        std::ofstream(fstab_) << content;
        MountedDrives::instance().set_fstab_path_for_test(fstab_.string());
    }

    static bool mounted(const std::string& path)
    {
        return MountedDrives::instance().drive_mounted(path);
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

TEST_F(FakeFstabTest, FstabPrefixEndsAtAPathComponent)  // [MountedDrivesTests.md#FstabPrefixEndsAtAPathComponent]
{
    fstab("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");

    EXPECT_FALSE(mounted(unplugged_ + "/a.txt"));
    EXPECT_FALSE(mounted(unplugged_)) << "der Einhaengepunkt selbst gehoert dazu";
    EXPECT_TRUE(mounted(unplugged_ + "2/a.txt")) << "ein Nachbar mit gleichem Namensanfang liegt nicht darunter";
}

TEST_F(FakeFstabTest, DeepestFstabMountpointDecides)  // [MountedDrivesTests.md#DeepestFstabMountpointDecides]
{
    fstab("proc /proc proc defaults 0 0\n"
          "UUID=0000-0000 /proc/sys ext4 defaults 0 2\n");

    EXPECT_TRUE(mounted("/proc/self/status")) << "/proc ist eingehaengt";
    EXPECT_FALSE(mounted("/proc/sys/kernel/hostname")) << "der tiefere Eintrag /proc/sys zaehlt, und der ist nicht eingehaengt";
}

TEST_F(FakeFstabTest, CommentsRootAndRelativeEntriesAreIgnored)  // [MountedDrivesTests.md#CommentsRootAndRelativeEntriesAreIgnored]
{
    fstab("# UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n"
          "UUID=1111-1111 / ext4 defaults 0 1\n"
          "UUID=2222-2222 mnt/afx_unplugged ext4 defaults 0 2\n"
          "\n");

    EXPECT_TRUE(mounted(unplugged_ + "/a.txt")) << "ohne gueltigen Eintrag gilt die Einhaengetabelle, und / ist eingehaengt";
}

TEST_F(FakeFstabTest, MissingFstabFallsBackToTheMountTable)
{
    MountedDrives::instance().set_fstab_path_for_test((root_ / "gibt-es-nicht").string());

    EXPECT_TRUE(mounted(fs::temp_directory_path().string()));
    EXPECT_TRUE(mounted(unplugged_ + "/a.txt"));
}

TEST_F(FakeFstabTest, MountTableAnswerIsCachedAndRefreshed)  // [MountedDrivesTests.md#MountTableAnswerIsCachedAndRefreshed]
{
    fstab("");
    auto& md = MountedDrives::instance();
    md.set_test_time(-11s);
    EXPECT_TRUE(md.drive_mounted(fs::temp_directory_path())) << "frisch aus /proc/mounts";
    md.set_test_time(1s);
    EXPECT_TRUE(md.drive_mounted(fs::temp_directory_path())) << "aus dem Cache";
    EXPECT_TRUE(md.drive_mounted("/proc/self/status")) << "ein anderer Einhaengepunkt aus demselben Cache";
}

TEST_F(FakeFstabTest, FileDifferenceTellsAnUnmountedDriveFromAMissingFile)  // [MountedDrivesTests.md#FileDifferenceTellsAnUnmountedDriveFromAMissingFile]
{
    fstab("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
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

TEST_F(FakeFstabTest, UnpluggedSourceDriveNeverDeletesTheTarget)  // [MountedDrivesTests.md#UnpluggedSourceDriveNeverDeletesTheTarget]
{
    fstab("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
    tu::write_text_file(root_ / "ziel.txt", "wertvoll");
    SyncItem si = file_item(unplugged("quelle.txt"), (root_ / "ziel.txt").wstring());
    si.delete_in_target(true);

    EXPECT_EQ(tu::run_sync(si), 0u);

    EXPECT_EQ(tu::read_text_file(root_ / "ziel.txt"), "wertvoll") << "ein abgezogenes Quelllaufwerk ist keine geloeschte Quelle";
}

TEST_F(FakeFstabTest, UnpluggedTargetDriveGetsNoCopy)
{
    fstab("UUID=0000-0000 " + unplugged_ + " ext4 defaults 0 2\n");
    tu::write_text_file(root_ / "quelle.txt", "neu");

    EXPECT_EQ(tu::run_sync(file_item((root_ / "quelle.txt").wstring(), unplugged("ziel.txt"))), 0u);

    EXPECT_FALSE(fs::exists(unplugged_)) << "nichts darf den fehlenden Einhaengepunkt als Verzeichnis anlegen";
}
#endif
