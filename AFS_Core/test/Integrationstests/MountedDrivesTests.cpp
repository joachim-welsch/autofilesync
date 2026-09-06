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
    //LOG_DEBUG(L"~~~ Start test: testGetMountPoint");
    fs::path mountPoint = getMountPoint(TESTMOUNTFN);
	EXPECT_EQ(mountPoint, TESTMOUNT2);
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckDriveMounted)
{
    //LOG_DEBUG(L"~~~ Start test: CheckDriveMounted");
    auto& md = MountedDrives::instance();
#ifdef _UNIX
    EXPECT_TRUE(md.driveMounted(TESTMOUNT));
#else
	EXPECT_TRUE(md.driveMounted(TESTMOUNT, L"winuser"));
#endif
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckTimeNotElapsed)
{
    //LOG_DEBUG(L"~~~ Start test: CheckTimeNotElapsed");
    auto& md = MountedDrives::instance();
    md.setTestTime(1s);
    EXPECT_FALSE(md.timeElapsedTest());
}

TEST(MountedDrivesTests, /*DISABLED_*/CheckTimeElapsed)
{
    //LOG_DEBUG(L"~~~ Start test: CheckTimeElapsed");
    auto& md = MountedDrives::instance();
    md.setTestTime(-11s);
    EXPECT_TRUE(md.timeElapsedTest());
}

// MountedDrives is a process-wide singleton called concurrently from the
// sync worker thread for every file compared; repeatedly forcing timeElapsed()
// to stale via setTestTime() interleaves refresh() (writer) with driveMounted()/
// check() (reader) calls from other threads, which races the shared MountInfos
// vectors and checkTime_ without the internal lock.
TEST(MountedDrivesTests, /*DISABLED_*/ConcurrentDriveMountedCallsDoNotCrash)
{
    auto& md = MountedDrives::instance();
    std::atomic<bool> stop{false};

    std::thread invalidator([&] {
        while (!stop.load())
            md.setTestTime(-11s);
    });

    std::vector<std::thread> checkers;
    for (int i = 0; i < 4; ++i) {
        checkers.emplace_back([&] {
            while (!stop.load()) {
#ifdef _UNIX
                md.driveMounted(TESTMOUNT);
#else
                md.driveMounted(TESTMOUNT, L"winuser");
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

// Fehler 3 (round 5): for a path under an unplugged drive, getMountRoot()
// walks up past the missing mountpoint to "/" - which is always mounted - so
// driveMounted() reported true and a missing source read as "file deleted"
// instead of "drive gone", which deleteInTarget acts on. Paths under a known
// fstab mountpoint are now judged by whether that mountpoint is actually
// mounted right now.
#if defined(_UNIX) && !defined(_MACOS)
TEST(MountedDrivesTests, FstabMountpointDecidesMountedState)
{
    namespace fs = std::filesystem;
    const fs::path fakeFstab = fs::temp_directory_path() / ("afx_fstab_" + std::to_string(getpid()));
    {
        std::ofstream out(fakeFstab);
        out << "UUID=0000-0000 /mnt/afx_testdrive ext4 defaults 0 2\n";
        out << "proc /proc proc defaults 0 0\n";
    }
    MountedDrives::instance().setFstabPathForTest(fakeFstab.string());
    // fstab knows the mountpoint, /proc/mounts does not have it -> not mounted.
    EXPECT_FALSE(MountedDrives::instance().driveMounted(L"/mnt/afx_testdrive/some/file.txt"));
    // fstab mountpoint that is really mounted -> mounted.
    EXPECT_TRUE(MountedDrives::instance().driveMounted(L"/proc/self/status"));
    // Path under no fstab mountpoint keeps the previous behavior.
    EXPECT_TRUE(MountedDrives::instance().driveMounted(fs::temp_directory_path()));
    MountedDrives::instance().setFstabPathForTest("/etc/fstab");
    fs::remove(fakeFstab);
}
#endif