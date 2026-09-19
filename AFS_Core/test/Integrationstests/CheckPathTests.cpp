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
#include "CheckPath.h"
#include "Config.h"
#include "SshAccount.h"
#include "SshTest.h"
#include "FileUtil.h"
#include "DirUtil.h"
#include <filesystem>
#include <thread>
#include <fstream>
#include <Util.h>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;
namespace {

    bool remove_all_retry(const fs::path& p, int attempts = 20)  // [CheckPathTests.md#remove_all_retry]
    {
        for (int i = 0; i < attempts; ++i) {
            std::error_code ec;
            fs::remove_all(p, ec);
            if (!fs::exists(p))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        return !fs::exists(p);
    }

    struct TempTree {
        fs::path path;
        explicit TempTree(fs::path p) : path(std::move(p)) { remove_all_retry(path); }
        ~TempTree() { remove_all_retry(path); }
    };

} // namespace

TEST(CheckPathTests, RepeatedHitsWithinValidityWindowDoNotExtendIt)  // [CheckPathTests.md#RepeatedHitsWithinValidityWindowDoNotExtendIt]
{
    using namespace std::chrono;
    constexpr auto VALIDITY = milliseconds(2000);
    TempTree tree(fs::temp_directory_path() /
        ("autofilesync_checkpath_test_" + std::to_string(util::current_pid())));

    const fs::path& base = tree.path;
    fs::remove_all(base);

    ASSERT_TRUE(fs::create_directories(base / "source"));
    ASSERT_TRUE(fs::create_directories(base / "target"));

    const std::wstring source = (base / "source").wstring();
    const std::wstring target = (base / "target").wstring();

    CheckPath check_path;
    const auto t0 = steady_clock::now();
    EXPECT_TRUE(check_path(source, target, false));

    ASSERT_TRUE(remove_all_retry(base / "source"))  // [CheckPathTests.md#stale-cache-detection]
        << "Quellverzeichnis liess sich nicht loeschen - Vorbedingung verletzt";
    
    for (int i = 0; i < 5; ++i) {  // [CheckPathTests.md#interval-must-not-grow]
        EXPECT_TRUE(check_path(source, target, false)) << "Durchlauf " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_LT(steady_clock::now() - t0, VALIDITY)
        << "Schleife hat das Gueltigkeitsfenster ueberschritten - "
        "Timing auf dieser Maschine zu unzuverlaessig fuer die Aussage";

    std::this_thread::sleep_until(t0 + VALIDITY + milliseconds(300));  // [CheckPathTests.md#absolute-deadline]
    EXPECT_FALSE(check_path(source, target, false));  // [CheckPathTests.md#windows-checks-path-itself]

    fs::remove_all(base);
}

TEST(CheckPathTests, UnreachableBackoffIsCappedFarBelowTheReachableOne)  // [CheckPathTests.md#UnreachableBackoffIsCappedFarBelowTheReachableOne]
{
    uint16_t interval = 2;
    for (int i = 0; i < 20; ++i)
        interval = next_check_interval(interval, true, false);
    EXPECT_EQ(interval, 300);
}

TEST(CheckPathTests, ReachableBackoffStopsAtItsLimitInsteadOfOvershooting)
{
    uint16_t interval = 2;
    for (int i = 0; i < 20; ++i)
        interval = next_check_interval(interval, true, true);
    EXPECT_EQ(interval, 3600);
}

TEST(CheckPathTests, BackoffGrowsByDoublingUntilTheCap)
{
    EXPECT_EQ(next_check_interval(2, true, false), 4);
    EXPECT_EQ(next_check_interval(128, true, false), 256);
    EXPECT_EQ(next_check_interval(256, true, false), 300);  // doubling would exceed the cap, so it is clamped to the cap
    EXPECT_EQ(next_check_interval(2048, true, true), 3600);
}

TEST(CheckPathTests, ChangedResultResetsBackoffToTheBaseInterval)  // [CheckPathTests.md#ChangedResultResetsBackoffToTheBaseInterval]
{
    EXPECT_EQ(next_check_interval(300, false, true), 2);
    EXPECT_EQ(next_check_interval(3600, false, false), 2);
}

TEST(CheckPathTests, WithoutVolumeSpecKeepsPathsThatMerelyContainParentheses)  // [CheckPathTests.md#WithoutVolumeSpecKeepsPathsThatMerelyContainParentheses]
{
    EXPECT_EQ(without_volume_spec(L"/home/user/Docs (2023)/file.txt"), L"/home/user/Docs (2023)/file.txt");
    EXPECT_EQ(without_volume_spec(L"/media/stick (dev-sdb1)"), L"/media/stick (dev-sdb1)");
    EXPECT_EQ(without_volume_spec(L"/plain/path/file.txt"), L"/plain/path/file.txt");
}

TEST(CheckPathTests, WithoutVolumeSpecStillStripsRealVolumeSpec)
{
    EXPECT_EQ(without_volume_spec(L"Volume (C:)\\dir\\file.txt"), L"C:\\dir\\file.txt");
}

namespace {

class CheckPathSshTest : public ::testing::Test {  // [CheckPathTests.md#CheckPathSshTest]
protected:
    void SetUp() override
    {
        Config::instance().reset();
        SshAccount unreachable;
        unreachable.set_name("unreachable");
        unreachable.set_user("nobody");
        unreachable.set_host("this-host-does-not-exist.invalid");
        unreachable.set_password("irrelevant");
        Config::instance().add(unreachable);

        SshAccount local;
        local.set_name("localhost");
        local.set_user(SSHTest::SSH_USER);
        local.set_host(SSHTest::SSH_HOST);
        local.set_password(SSHTest::SSH_PASSWORD);
        Config::instance().add(local);
    }

    void TearDown() override { Config::instance().reset(); }
};

} // namespace

TEST_F(CheckPathSshTest, UnreachableHostIsNotReachable)  // [CheckPathTests.md#UnreachableHostIsNotReachable]
{
    CheckPath check_path;
    EXPECT_FALSE(check_path.ssh_path_reachable(L"unreachable:/tmp", true));
    EXPECT_FALSE(check_path.ssh_path_reachable(L"unreachable:/tmp", false))
        << "auch ohne Existenzforderung muss die Verbindung selbst zustande kommen";
}

TEST_F(CheckPathSshTest, UnknownAccountIsNotReachable)  // [CheckPathTests.md#UnknownAccountIsNotReachable]
{
    CheckPath check_path;
    EXPECT_FALSE(check_path.ssh_path_reachable(L"gibtesnicht:/tmp", true));
}

TEST_F(CheckPathSshTest, OperatorReportsUnreachableSshSource)  // [CheckPathTests.md#OperatorReportsUnreachableSshSource]
{
    CheckPath check_path;
    const std::wstring target = tu::temp_dir_w();
    EXPECT_FALSE(check_path(L"unreachable:/remote/quelle", target, true));
}

TEST_F(CheckPathSshTest, ExistingRemotePathIsReachable)
{
    SKIP_WITHOUT_SSH_SERVER();
    const std::string dir = tu::concat_path(tu::test_temp_dir(), "afx_checkpath_ssh");  // [CheckPathTests.md#create-dir-instead-of-assuming]
    fs::create_directories(dir);
    const std::wstring remote = L"localhost:" + util::string_to_wstring(dir);

    CheckPath check_path;
    EXPECT_TRUE(check_path.ssh_path_reachable(remote, true)) << util::wstring_to_string(remote);

    EXPECT_TRUE(check_path.ssh_path_reachable(remote + L"\\", true)) << "mit abschliessendem Backslash";  // [CheckPathTests.md#trailing-separator]
    EXPECT_TRUE(check_path.ssh_path_reachable(remote + L"/", true)) << "mit abschliessendem Schraegstrich";

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_F(CheckPathSshTest, MissingRemoteSourcePathIsNotReachable)  // [CheckPathTests.md#MissingRemoteSourcePathIsNotReachable]
{
    SKIP_WITHOUT_SSH_SERVER();
    CheckPath check_path;
    const std::wstring missing = L"localhost:" + tu::temp_dir_w() + L"/gibt_es_sicher_nicht_4711";
    EXPECT_FALSE(check_path.ssh_path_reachable(missing, true));
}

TEST_F(CheckPathSshTest, MissingRemoteTargetPathIsStillReachable)  // [CheckPathTests.md#MissingRemoteTargetPathIsStillReachable]
{
    SKIP_WITHOUT_SSH_SERVER();
    CheckPath check_path;
    const std::wstring missing = L"localhost:" + tu::temp_dir_w() + L"/gibt_es_sicher_nicht_4711";
    EXPECT_TRUE(check_path.ssh_path_reachable(missing, false));
}
