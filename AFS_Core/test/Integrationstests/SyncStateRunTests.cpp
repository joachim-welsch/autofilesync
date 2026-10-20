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
#include "ScopedDeleteMode.h"
#include "ScopedEnv.h"
#include "ScopedLoggingSetup.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "Util.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;
using std::chrono::milliseconds;

class SyncStateRunTest: public ::testing::Test {  // [SyncStateRunTests.md#SyncStateRunTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / "autofilesync_syncstate_run_test";
    const fs::path config_home_ = root_ / "config";
    const fs::path source_dir_ = root_ / "source";
    const fs::path target_dir_ = root_ / "target";
    tu::ScopedLoggingSetup logging_;
    std::unique_ptr<ScopedEnv> env_;
    std::unique_ptr<tu::ScopedDeleteMode> delete_mode_;  // after the reset, which would bring the recycle bin back

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        env_ = std::make_unique<ScopedEnv>("XDG_CONFIG_HOME", config_home_.string().c_str());
        Logging::instance().set_log_file_path_name((root_ / "test.log").wstring());
        Config::instance().reset();
        delete_mode_ = std::make_unique<tu::ScopedDeleteMode>(DeleteMode::Permanent);
    }
    void TearDown() override
    {
        Synchronize::destroy();
        delete_mode_.reset();
        Config::instance().reset();
        env_.reset();
        fs::remove_all(root_);
    }
    SyncItem two_way_tree(bool two_way = true) const
    {
        SyncItem si(L"beide Seiten", SyncItemType::Tree, source_dir_.wstring(), target_dir_.wstring(), L"");
        si.two_way_sync(two_way);
        si.sync_cycle(0);
        return si;
    }
    fs::path state_file() const { return sync_state_file_path(sync_state_dir(), L"beide Seiten"); }
    std::optional<SyncState> stored_state() const { return read_sync_state(state_file(), source_dir_.wstring(), target_dir_.wstring()); }

    void run_pass(SyncType type)  // [SyncStateRunTests.md#run_pass]
    {
        const auto before = Config::instance().syncitems()[0].last_sync();
        Synchronize::create(type);
        if (type == SyncType::Single)
            for (int i = 0; i < 500 && Config::instance().syncitems()[0].last_sync() == before; ++i)
                std::this_thread::sleep_for(milliseconds(20));
        else
            std::this_thread::sleep_for(milliseconds(300));
        Synchronize::destroy();
    }
};

}

TEST_F(SyncStateRunTest, ARunWritesTheStateNextToTheConfigurationAndTheNextRunUsesIt)  // [SyncStateRunTests.md#round-trip]
{
    tu::write_text_file(source_dir_ / "gone.txt", "synced once", seconds(3600));
    tu::write_text_file(source_dir_ / "sub" / "kept.txt", "stays", seconds(3600));
    Config::instance().add(two_way_tree());

    run_pass(SyncType::Single);

    ASSERT_TRUE(fs::exists(state_file())) << state_file();
    EXPECT_EQ(state_file().parent_path(), config_home_ / "autofilesync" / "state");
    std::optional<SyncState> state = stored_state();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->files().size(), 2u);
    const auto written = fs::last_write_time(state_file());
    EXPECT_EQ(Config::instance().syncitems()[0].last_run()->known_files, 2u);

    fs::remove(source_dir_ / "gone.txt");
    run_pass(SyncType::Single);

    EXPECT_FALSE(fs::exists(target_dir_ / "gone.txt")) << "the second run must know the file was deleted";
    state = stored_state();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->files().size(), 1u);
    EXPECT_NE(state->find(L"sub/kept.txt"), nullptr);
    EXPECT_EQ(Config::instance().syncitems()[0].last_run()->known_files, 1u);

    EXPECT_FALSE(util::equal(fs::last_write_time(state_file()), written)) << "the deletion changed the state";
    const auto settled = fs::last_write_time(state_file());
    run_pass(SyncType::Single);
    EXPECT_TRUE(util::equal(fs::last_write_time(state_file()), settled)) << "a run that changes nothing must not rewrite the state";
}

TEST_F(SyncStateRunTest, AOneWayItemHasNoState)
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    Config::instance().add(two_way_tree(false));

    run_pass(SyncType::Single);

    EXPECT_TRUE(fs::exists(target_dir_ / "a.txt"));
    EXPECT_FALSE(fs::exists(state_file()));
    EXPECT_FALSE(fs::exists(sync_state_dir()));
    EXPECT_FALSE(Config::instance().syncitems()[0].last_run()->known_files.has_value());
}

TEST_F(SyncStateRunTest, CountingAndPreviewPassesWriteNoState)  // [SyncStateRunTests.md#touches-nothing]
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    Config::instance().add(two_way_tree());

    run_pass(SyncType::CountFiles);
    EXPECT_FALSE(fs::exists(state_file()));
    run_pass(SyncType::Preview);
    EXPECT_FALSE(fs::exists(state_file()));
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
}

TEST_F(SyncStateRunTest, AStoppedRunKeepsTheEntriesItDidNotReach)  // [SyncStateRunTests.md#stopped-run]
{
    constexpr std::size_t FILES = 200;
    for (std::size_t i = 0; i < FILES; ++i)
        tu::write_text_file(source_dir_ / ("file" + std::to_string(i) + ".txt"), std::string(200000, 'x'), seconds(3600));
    Config::instance().add(two_way_tree());
    run_pass(SyncType::Single);
    ASSERT_EQ(stored_state()->files().size(), FILES);
    for (std::size_t i = 0; i < FILES; ++i)
        tu::write_text_file(source_dir_ / ("file" + std::to_string(i) + ".txt"), std::string(200000, 'y'));
    const auto edited = fs::last_write_time(source_dir_ / "file0.txt");
    const auto copied = [&] {  // the run is writing while this counts, so a vanished temp file must not throw
        std::error_code ec;
        std::size_t count = 0;
        for (const fs::directory_entry& e : fs::directory_iterator(target_dir_, ec))
            if (const auto time = fs::last_write_time(e.path(), ec); !ec && time >= edited)
                ++count;
        return count;
    };

    Synchronize::create(SyncType::Single);
    for (int i = 0; i < 2000 && copied() == 0; ++i)
        std::this_thread::sleep_for(milliseconds(1));
    Synchronize::stop();
    Synchronize::destroy();

    const std::optional<SyncState> state = stored_state();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->files().size(), FILES) << "the files the stopped run did not reach must stay known";
    std::cout << "stopped after " << copied() << " of " << FILES << " files\n";
}

TEST_F(SyncStateRunTest, TheStateLeavesWithItsItem)
{
    tu::write_text_file(source_dir_ / "a.txt", "a");
    Config::instance().add(two_way_tree());
    run_pass(SyncType::Single);
    ASSERT_TRUE(fs::exists(state_file()));

    EXPECT_TRUE(Config::instance().erase_by_name(L"beide Seiten"));

    EXPECT_FALSE(fs::exists(state_file()));
}

TEST_F(SyncStateRunTest, AOneWayItemTrackingMovesHasAStateAndRenames)
{
    tu::write_text_file(source_dir_ / "a.txt", "one-way move", seconds(3600));
    SyncItem si = two_way_tree(false);
    si.delete_in_target(true);
    si.track_moves(true);
    Config::instance().add(si);
    run_pass(SyncType::Single);
    ASSERT_TRUE(fs::exists(state_file()));
    fs::rename(source_dir_ / "a.txt", source_dir_ / "b.txt");

    run_pass(SyncType::Single);

    EXPECT_TRUE(fs::exists(target_dir_ / "b.txt"));
    EXPECT_FALSE(fs::exists(target_dir_ / "a.txt"));
    EXPECT_EQ(Config::instance().syncitems()[0].last_run()->known_files, 1u);
}
