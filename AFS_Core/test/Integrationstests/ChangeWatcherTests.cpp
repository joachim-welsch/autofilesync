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
#include "ChangeWatcher.h"
#include "FileUtil.h"
#include "ScopedLoggingSetup.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>

using namespace AFS_Core;

namespace fs = std::filesystem;
using std::chrono::milliseconds;

namespace {
    class ChangeWatcherTest: public ::testing::Test {  // [ChangeWatcherTests.md#ChangeWatcherTest]
    protected:
        const fs::path root_ = fs::temp_directory_path() / "autofilesync_changewatcher_test";
        std::optional<tu::ScopedLoggingSetup> logging_;  // [ChangeWatcherTests.md#log-in-test-dir]
        std::optional<ChangeWatcher> watcher_;
        void SetUp() override
        {
            fs::remove_all(root_);
            fs::create_directories(root_ / "tree" / "sub");
            fs::create_directories(root_ / "flat");
            logging_.emplace();
            Logging::instance().set_log_file_path_name((root_ / "test.log").wstring());
        }
        void TearDown() override
        {
            watcher_.reset();
            logging_.reset();
            fs::remove_all(root_);
        }

        ChangeWatcher& watch(const std::vector<WatchedDir>& dirs)  // [ChangeWatcherTests.md#watch]
        {
            std::this_thread::sleep_for(milliseconds(100));
            return watcher_.emplace(dirs);
        }

        static bool noticed(const ChangeWatcher& watcher, const std::wstring& item, int timeout_ms = 3000)  // [ChangeWatcherTests.md#noticed]
        {
            for (int waited = 0; waited < timeout_ms; waited += 20) {
                if (watcher.last_change(item))
                    return true;
                std::this_thread::sleep_for(milliseconds(20));
            }
            return false;
        }
    };
}

TEST_F(ChangeWatcherTest, AFileWrittenBelowAWatchedTreeIsNoticed)
{
    ChangeWatcher& watcher = watch({{L"baum", root_ / "tree", true}});
    ASSERT_TRUE(ChangeWatcher::available());
    ASSERT_TRUE(watcher.watching(L"baum"));
    EXPECT_FALSE(watcher.last_change(L"baum").has_value()) << "nothing happened yet";

    tu::write_text_file(root_ / "tree" / "sub" / "neu.txt", "x");

    EXPECT_TRUE(noticed(watcher, L"baum"));
    watcher.clear(L"baum");
    EXPECT_FALSE(watcher.last_change(L"baum").has_value());
}

TEST_F(ChangeWatcherTest, ADirectoryCreatedLaterIsWatchedToo)  // [ChangeWatcherTests.md#ADirectoryCreatedLaterIsWatchedToo]
{
    ChangeWatcher& watcher = watch({{L"baum", root_ / "tree", true}});
    fs::create_directories(root_ / "tree" / "spaeter" / "tief");
    ASSERT_TRUE(noticed(watcher, L"baum"));
    std::this_thread::sleep_for(milliseconds(200));
    watcher.clear(L"baum");

    tu::write_text_file(root_ / "tree" / "spaeter" / "tief" / "datei.txt", "x");

    EXPECT_TRUE(noticed(watcher, L"baum")) << "a file in a directory that appeared after the start must be seen";
}

TEST_F(ChangeWatcherTest, AFlatWatchIgnoresSubdirectories)
{
    fs::create_directories(root_ / "flat" / "sub");
    ChangeWatcher& watcher = watch({{L"flach", root_ / "flat", false}});
    tu::write_text_file(root_ / "flat" / "sub" / "tief.txt", "x");
    EXPECT_FALSE(noticed(watcher, L"flach", 1000)) << "a directory item has no subdirectories";

    tu::write_text_file(root_ / "flat" / "oben.txt", "x");
    EXPECT_TRUE(noticed(watcher, L"flach"));
}

TEST_F(ChangeWatcherTest, DeletionsAndRenamesCountAsChanges)
{
    tu::write_text_file(root_ / "tree" / "a.txt", "x");
    ChangeWatcher& watcher = watch({{L"baum", root_ / "tree", true}});
    fs::rename(root_ / "tree" / "a.txt", root_ / "tree" / "b.txt");
    EXPECT_TRUE(noticed(watcher, L"baum"));
    watcher.clear(L"baum");
    fs::remove(root_ / "tree" / "b.txt");
    EXPECT_TRUE(noticed(watcher, L"baum"));
}

TEST_F(ChangeWatcherTest, AMissingDirectoryIsNotWatched)  // [ChangeWatcherTests.md#AMissingDirectoryIsNotWatched]
{
    ChangeWatcher& watcher = watch({{L"weg", root_ / "gibt-es-nicht", true}, {L"da", root_ / "flat", false}});
    EXPECT_FALSE(watcher.watching(L"weg"));
    EXPECT_TRUE(watcher.watching(L"da"));
    EXPECT_FALSE(watcher.watching(L"unbekannt"));
}

TEST_F(ChangeWatcherTest, TwoItemsAreKeptApart)
{
    ChangeWatcher& watcher = watch({{L"baum", root_ / "tree", true}, {L"flach", root_ / "flat", false}});
    tu::write_text_file(root_ / "flat" / "x.txt", "x");
    EXPECT_TRUE(noticed(watcher, L"flach"));
    EXPECT_FALSE(watcher.last_change(L"baum").has_value()) << "a change in one item's directory must not mark the other";
}
