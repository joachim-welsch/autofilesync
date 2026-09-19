/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "MountedDrivesTests.h"
#include <MountedDrives.h>
#include <filesystem>
#include <Logging.h>
#include <thread>
#include <atomic>
#include <vector>

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