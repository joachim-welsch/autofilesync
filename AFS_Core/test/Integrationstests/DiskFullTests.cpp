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
#include "CopyLog.h"
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#include <RecycleBin.h>
#include <StrUtil.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

constexpr std::size_t MEBIBYTE = 1u << 20;

class FreeSpace {  // [DiskFullTests.md#FreeSpace]
public:
    explicit FreeSpace(std::uint64_t bytes) { fsu::set_free_space_for_test(bytes); }
    FreeSpace(const FreeSpace&) = delete;
    FreeSpace& operator=(const FreeSpace&) = delete;
    ~FreeSpace() { fsu::set_free_space_for_test(std::nullopt); }
};

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class DiskFullTest : public ::testing::Test {  // [DiskFullTests.md#DiskFullTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_disk_full_" + std::to_string(TestPaths::process_id()));
    const fs::path source_dir_ = root_ / "quelle";
    const fs::path target_dir_ = root_ / "ziel";

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(source_dir_);
        fs::create_directories(target_dir_);
        Synchronize::start();
    }

    void TearDown() override
    {
        fsu::set_free_space_for_test(std::nullopt);
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem item(SyncItemType type) const
    {
        CompareItems ci;
        ci.content(false);
        ci.time(true);
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(type);
        si.source(source_dir_.wstring());
        si.target(target_dir_.wstring());
        si.compare_items(ci);
        return si;
    }

    SyncItem file_item(const char* name) const
    {
        SyncItem si = item(SyncItemType::File);
        si.source((source_dir_ / name).wstring());
        si.target((target_dir_ / name).wstring());
        return si;
    }

    static void run(const SyncItem& si, Outcome& outcome, SyncState* state = nullptr)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }

    void write_three_large_files_with_old_targets() const
    {
        for (const char* name : {"a.bin", "b.bin", "c.bin"}) {
            tu::write_large_file(source_dir_ / name, 3 * MEBIBYTE);
            tu::write_text_file(target_dir_ / name, "alt", seconds(3600));
        }
    }
};

} // namespace

TEST_F(DiskFullTest, FileThatDoesNotFitIsReportedAndTheOldTargetIsNotTouched)  // [DiskFullTests.md#FileThatDoesNotFitIsReportedAndTheOldTargetIsNotTouched]
{
    tu::write_large_file(source_dir_ / "a.bin", 3 * MEBIBYTE);
    tu::write_text_file(target_dir_ / "a.bin", "alt", seconds(3600));
    const auto target_time = fs::last_write_time(target_dir_ / "a.bin");
    Outcome outcome;
    {
        const FreeSpace one_mebibyte(MEBIBYTE);
        run(file_item("a.bin"), outcome);
    }

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::DiskFull)), std::wstring::npos) << "the error says that the disk is full";
    EXPECT_NE(outcome.report.errors().front().find((target_dir_ / "a.bin").wstring()), std::wstring::npos) << "and names the file that did not fit";
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.bin"), "alt");
    EXPECT_EQ(fs::last_write_time(target_dir_ / "a.bin"), target_time);
    EXPECT_EQ(names_in(target_dir_), std::set<std::string>{"a.bin"}) << "no .bak and no half file";
}

TEST_F(DiskFullTest, FileThatJustFitsIsCopied)  // [DiskFullTests.md#FileThatJustFitsIsCopied]
{
    tu::write_large_file(source_dir_ / "a.bin", MEBIBYTE);
    Outcome outcome;
    {
        const FreeSpace exactly_enough(MEBIBYTE);
        run(file_item("a.bin"), outcome);
    }

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_TRUE(outcome.report.successful());
    EXPECT_EQ(fs::file_size(target_dir_ / "a.bin"), MEBIBYTE);
}

TEST_F(DiskFullTest, DirectoryPassCopiesWhatFitsAndSumsUpTheRestInOneError)  // [DiskFullTests.md#DirectoryPassCopiesWhatFitsAndSumsUpTheRestInOneError]
{
    for (const SyncItemType type : {SyncItemType::Dir, SyncItemType::Tree}) {
        fs::remove_all(source_dir_);
        fs::remove_all(target_dir_);
        fs::create_directories(target_dir_);
        write_three_large_files_with_old_targets();
        tu::write_text_file(source_dir_ / "klein.txt", "passt");
        Outcome outcome;
        {
            const FreeSpace one_mebibyte(MEBIBYTE);
            run(item(type), outcome);
        }

        EXPECT_EQ(outcome.copies, 1u) << "the small file still fits: " << static_cast<int>(type);
        EXPECT_EQ(tu::read_text_file(target_dir_ / "klein.txt"), "passt");
        ASSERT_EQ(outcome.report.errors().size(), 1u) << "three files, one cause, one error: " << static_cast<int>(type);
        EXPECT_NE(outcome.report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::DiskFull)), std::wstring::npos) << outcome.report.errors().front();
        for (const char* name : {"a.bin", "b.bin", "c.bin"})
            EXPECT_EQ(tu::read_text_file(target_dir_ / name), "alt") << name;
        EXPECT_EQ(names_in(target_dir_), (std::set<std::string>{"a.bin", "b.bin", "c.bin", "klein.txt"}));
    }
}

TEST_F(DiskFullTest, PassAfterSpaceWasMadeCopiesEverything)  // [DiskFullTests.md#PassAfterSpaceWasMadeCopiesEverything]
{
    write_three_large_files_with_old_targets();
    Outcome full, freed;
    {
        const FreeSpace one_mebibyte(MEBIBYTE);
        run(item(SyncItemType::Dir), full);
    }
    ASSERT_EQ(full.copies, 0u);

    run(item(SyncItemType::Dir), freed);

    EXPECT_EQ(freed.copies, 3u);
    EXPECT_TRUE(freed.report.successful());
    EXPECT_EQ(fs::file_size(target_dir_ / "c.bin"), 3 * MEBIBYTE);
}

TEST_F(DiskFullTest, TwoWayCopyBackThatDoesNotFitKeepsTheSource)  // [DiskFullTests.md#TwoWayCopyBackThatDoesNotFitKeepsTheSource]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_text_file(source_dir_ / "a.bin", "alt", seconds(3600));
    tu::write_large_file(target_dir_ / "a.bin", 3 * MEBIBYTE);
    SyncItem si = file_item("a.bin");
    si.two_way_sync(true);
    SyncState state(si.source(), si.target());
    Outcome outcome;
    {
        const FreeSpace one_mebibyte(MEBIBYTE);
        run(si, outcome, &state);
    }

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find((source_dir_ / "a.bin").wstring()), std::wstring::npos) << "the side that is full is the source";
    EXPECT_EQ(tu::read_text_file(source_dir_ / "a.bin"), "alt");
    EXPECT_EQ(names_in(source_dir_), std::set<std::string>{"a.bin"});
    EXPECT_EQ(state.find(L"a.bin"), nullptr) << "a file that was not brought in sync is not recorded as such";
}

TEST_F(DiskFullTest, DeletionsStillHappenOnAFullDisk)  // [DiskFullTests.md#DeletionsStillHappenOnAFullDisk]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    tu::write_large_file(source_dir_ / "neu.bin", 3 * MEBIBYTE);
    tu::write_text_file(target_dir_ / "weg.txt", "nur im Ziel", seconds(3600));
    SyncItem si = item(SyncItemType::Dir);
    si.delete_in_target(true);
    Outcome outcome;
    {
        const FreeSpace one_mebibyte(MEBIBYTE);
        run(si, outcome);
    }

    EXPECT_FALSE(fs::exists(target_dir_ / "weg.txt")) << "deleting is what makes room";
    EXPECT_EQ(outcome.report.errors().size(), 1u);
}

#ifdef __linux__
TEST_F(DiskFullTest, ADeletionTheRecycleBinHasNoRoomForNamesTheBin)  // [DiskFullTests.md#ADeletionTheRecycleBinHasNoRoomForNamesTheBin]
{
    const fs::path bin = fs::path("/dev/shm") / ("autofilesync_disk_full_bin_" + std::to_string(TestPaths::process_id()));
    std::error_code ec;
    if (fs::rename(target_dir_, bin, ec); !ec || ec != std::errc::cross_device_link) {
        fs::remove_all(bin, ec);
        GTEST_SKIP() << "kein zweites Dateisystem unter /dev/shm";
    }
    const tu::ScopedDeleteMode delete_mode(DeleteMode::RecycleBin);
    const fs::path previous_bin = RecycleBin::instance().bin_path();
    RecycleBin::instance().bin_path(bin);
    tu::write_text_file(target_dir_ / "weg.txt", "nur im Ziel", seconds(3600));
    SyncItem si = item(SyncItemType::Dir);
    si.delete_in_target(true);
    Outcome outcome;
    {
        const FreeSpace three_bytes(3);
        run(si, outcome);
    }
    RecycleBin::instance().bin_path(previous_bin);
    const bool bin_made = fs::exists(bin / "weg.txt");
    fs::remove_all(bin, ec);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    const std::wstring& error = outcome.report.errors().front();
    EXPECT_NE(error.find(translation::translate("target_delete_failed") + (target_dir_ / "weg.txt").wstring()), std::wstring::npos) << util::wstring_to_string(error);
    EXPECT_NE(error.find(translation::translate("recycle_bin_full")), std::wstring::npos) << util::wstring_to_string(error);
    EXPECT_TRUE(fs::exists(target_dir_ / "weg.txt"));
    EXPECT_FALSE(bin_made);
}
#endif

TEST(SyncReportShortage, FirstShortageNamesItsFileAndLaterOnesAreSummedUpInItsPlace)  // [DiskFullTests.md#SyncReportShortage]
{
    SyncReport report;
    report.error(L"davor");
    report.shortage(L"/ziel/a.bin", 3 * MEBIBYTE, MEBIBYTE);
    const std::wstring single = report.errors().back();
    report.error(L"danach");
    report.shortage(L"/ziel/b.bin", 2 * MEBIBYTE, MEBIBYTE);
    report.shortage(L"/ziel/c.bin", MEBIBYTE + 1, MEBIBYTE);

    ASSERT_EQ(report.errors().size(), 3u) << "three shortages are one error";
    EXPECT_EQ(report.errors()[0], L"davor");
    EXPECT_EQ(report.errors()[2], L"danach");
    EXPECT_NE(single.find(L"/ziel/a.bin"), std::wstring::npos);
    EXPECT_NE(single.find(fsu::make_error_message(fsu::FileInfoResult::DiskFull)), std::wstring::npos);
    EXPECT_NE(report.errors()[1], single) << "the summary replaces the first message";
    EXPECT_NE(report.errors()[1].find(fsu::make_error_message(fsu::FileInfoResult::DiskFull)), std::wstring::npos);
    EXPECT_FALSE(report.successful());
}

TEST(SyncReportShortage, ShortageBehindTheErrorLimitIsNotLost)  // [DiskFullTests.md#SyncReportShortage]
{
    SyncReport report;
    for (std::size_t i = 0; i <= SyncReport::MAX_ERRORS; ++i)
        report.error(L"voll");
    const std::size_t capped = report.errors().size();

    report.shortage(L"/ziel/a.bin", 3 * MEBIBYTE, MEBIBYTE);
    report.shortage(L"/ziel/b.bin", 3 * MEBIBYTE, MEBIBYTE);

    EXPECT_EQ(report.errors().size(), capped) << "the cap holds";
    EXPECT_FALSE(report.successful());
}
