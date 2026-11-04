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
#include "ScopedConfigHome.h"
#include "ScopedLoggingSetup.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "Util.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;
using std::chrono::milliseconds;

class SyncStateRunTest: public ::testing::Test {  // [SyncStateRunTests.md#SyncStateRunTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / "autofilesync_syncstate_run_test";
    const fs::path source_dir_ = root_ / "source";
    const fs::path target_dir_ = root_ / "target";
    std::optional<tu::ScopedLoggingSetup> logging_;  // [SyncStateRunTests.md#SyncStateRunTest]
    std::optional<tu::ScopedConfigHome> config_home_;
    std::unique_ptr<tu::ScopedDeleteMode> delete_mode_;  // after the reset, which would bring the recycle bin back

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        config_home_.emplace(root_ / "config");
        logging_.emplace();
        Logging::instance().set_log_file_path_name((root_ / "test.log").wstring());
        Config::instance().reset();
        delete_mode_ = std::make_unique<tu::ScopedDeleteMode>(DeleteMode::Permanent);
    }
    void TearDown() override
    {
        Synchronize::destroy();
        delete_mode_.reset();
        Config::instance().reset();
        config_home_.reset();
        logging_.reset();
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

    void run_pass(SyncType type, bool mass_deletion_approved = false)  // [SyncStateRunTests.md#run_pass]
    {
        const auto before = Config::instance().syncitems()[0].last_sync();
        Synchronize::create(type, nullptr, SyncRole::Foreground, mass_deletion_approved);
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
    EXPECT_EQ(state_file().parent_path(), config_home_->config_dir() / "state");
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

TEST_F(SyncStateRunTest, AOneWayItemThatDeletesRefusesAMassDeletion)  // [SyncStateRunTests.md#one-way-mass-deletion]
{
    const auto files_in = [](const fs::path& dir) { return std::distance(fs::directory_iterator(dir), fs::directory_iterator()); };
    for (int i = 0; i < 12; ++i)
        tu::write_text_file(source_dir_ / ("datei" + std::to_string(i) + ".txt"), "x", seconds(3600));
    SyncItem si = two_way_tree(false);
    si.delete_in_target(true);
    Config::instance().add(si);
    run_pass(SyncType::Single);
    ASSERT_EQ(files_in(target_dir_), 12);

    fs::remove(source_dir_ / "datei0.txt");
    run_pass(SyncType::Single);
    EXPECT_EQ(files_in(target_dir_), 11) << "a single deletion goes through as before";

    fs::remove_all(source_dir_);
    fs::create_directories(source_dir_);
    run_pass(SyncType::Single);
    EXPECT_EQ(files_in(target_dir_), 11) << "an ordinary run must refuse to empty the target";
    EXPECT_EQ(Config::instance().syncitems()[0].last_run()->errors, 1u) << "the refusal is an error of the run";
    EXPECT_FALSE(fs::exists(state_file())) << "the notebook of a one-way item is never written";
    EXPECT_FALSE(Config::instance().syncitems()[0].last_run()->known_files.has_value()) << "a one-way item tracks no deletions between runs";

    run_pass(SyncType::Single, true);
    EXPECT_EQ(files_in(target_dir_), 0) << "the approved run deletes";
}

TEST_F(SyncStateRunTest, AOneWayItemThatDeletesTakesNoCopyForARename)  // [SyncStateRunTests.md#one-way-no-renames]
{
    tu::write_text_file(source_dir_ / "a.txt", "same", seconds(3600));
    SyncItem si = two_way_tree(false);
    si.delete_in_target(true);
    Config::instance().add(si);
    run_pass(SyncType::Single);
    ASSERT_TRUE(fs::exists(target_dir_ / "a.txt"));
    fs::copy_file(source_dir_ / "a.txt", source_dir_ / "b.txt");
    fs::last_write_time(source_dir_ / "b.txt", fs::last_write_time(source_dir_ / "a.txt"));

    run_pass(SyncType::Single);

    EXPECT_TRUE(fs::exists(target_dir_ / "a.txt")) << "a.txt is still in the source; it must not be renamed to b.txt";
    EXPECT_TRUE(fs::exists(target_dir_ / "b.txt"));
}

TEST_F(SyncStateRunTest, AMassDeletionWaitsForTheApprovedRun)  // [SyncStateRunTests.md#mass-deletion]
{
    for (int i = 0; i < 12; ++i)
        tu::write_text_file(source_dir_ / ("datei" + std::to_string(i) + ".txt"), "x", seconds(3600));
    Config::instance().add(two_way_tree());
    run_pass(SyncType::Single);
    ASSERT_EQ(stored_state()->files().size(), 12u);
    fs::remove_all(target_dir_);
    fs::create_directories(target_dir_);

    run_pass(SyncType::Single);
    EXPECT_EQ(std::distance(fs::directory_iterator(source_dir_), fs::directory_iterator()), 12) << "an ordinary run must refuse";
    EXPECT_EQ(Config::instance().syncitems()[0].last_run()->errors, 1u) << "the refusal is an error of the run";

    run_pass(SyncType::Single, true);
    EXPECT_EQ(std::distance(fs::directory_iterator(source_dir_), fs::directory_iterator()), 0) << "the approved run deletes";
    EXPECT_TRUE(stored_state()->empty());
}
