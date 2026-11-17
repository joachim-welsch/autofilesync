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
#ifdef _WIN32
#include <Windows.h>
#include "CopyLog.h"
#include "FileDifference.h"
#include "FileUtil.h"
#include "ScopedDeleteMode.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#include <chrono>
#include <filesystem>
#include <set>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

class ExclusiveHandle {  // [LockedFileTests.md#ExclusiveHandle]
    HANDLE handle_;
public:
    explicit ExclusiveHandle(const fs::path& file)
        : handle_(CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
    {}
    ~ExclusiveHandle()
    {
        if (held())
            CloseHandle(handle_);
    }
    ExclusiveHandle(const ExclusiveHandle&) = delete;
    ExclusiveHandle& operator=(const ExclusiveHandle&) = delete;
    bool held() const { return handle_ != INVALID_HANDLE_VALUE; }
};

class LockedFileTest : public ::testing::Test {  // [LockedFileTests.md#LockedFileTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_locked_" + std::to_string(TestPaths::process_id()));
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
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    SyncItem file_item(bool by_content) const
    {
        CompareItems ci;
        ci.content(by_content);
        ci.time(!by_content);
        ci.size(false);
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source((source_dir_ / "a.txt").wstring());
        si.target((target_dir_ / "a.txt").wstring());
        si.compare_items(ci);
        return si;
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }

    static bool names(const SyncReport& report, const fs::path& file)
    {
        return report.errors().size() == 1 && report.errors().front().find(file.wstring()) != std::wstring::npos
            && report.errors().front().find(translation::translate("file_locked")) != std::wstring::npos;
    }
};

} // namespace

TEST_F(LockedFileTest, LockedFileIsReportedAsLockedNotAsDifferent)  // [LockedFileTests.md#LockedFileIsReportedAsLockedNotAsDifferent]
{
    tu::write_text_file(source_dir_ / "a.txt", "gleich");
    tu::write_text_file(target_dir_ / "a.txt", "gleich");
    const std::wstring open = (source_dir_ / "a.txt").wstring();
    const std::wstring locked = (target_dir_ / "a.txt").wstring();
    uint64_t filesize = 0;
    fs::file_time_type last_write_time;
    const ExclusiveHandle lock(target_dir_ / "a.txt");
    ASSERT_TRUE(lock.held());

    const fsu::ContentComparison comparison = fsu::compare_content(open, locked);

    EXPECT_EQ(fsu::read_refusal(locked), fsu::FileInfoResult::FileLocked);
    EXPECT_EQ(fsu::read_refusal(open), fsu::FileInfoResult::OK);
    EXPECT_FALSE(comparison.equal);
    EXPECT_EQ(comparison.refusal, fsu::FileInfoResult::FileLocked);
    EXPECT_EQ(comparison.refused_file, locked);
    EXPECT_EQ(fsu::fileinfo(locked, filesize, last_write_time), fsu::FileInfoResult::OK) << "size and time of a locked file can still be asked for";
}

TEST_F(LockedFileTest, ContentComparisonWithALockedFileChangesNothing)  // [LockedFileTests.md#ContentComparisonWithALockedFileChangesNothing]
{
    const tu::ScopedDeleteMode delete_mode(DeleteMode::Permanent);
    for (const bool two_way : {false, true})
        for (const bool source_locked : {true, false}) {
            tu::write_text_file(source_dir_ / "a.txt", "gleich", seconds(3600));
            tu::write_text_file(target_dir_ / "a.txt", "gleich", seconds(3600));
            fs::last_write_time(target_dir_ / "a.txt", fs::last_write_time(source_dir_ / "a.txt"));
            SyncItem si = file_item(true);
            si.two_way_sync(two_way);
            si.conflict_copies(two_way);
            SyncState state(si.source(), si.target());
            const fs::path locked = (source_locked ? source_dir_ : target_dir_) / "a.txt";
            SyncReport report;
            std::size_t copies = 0;
            {
                const ExclusiveHandle lock(locked);
                ASSERT_TRUE(lock.held());
                SyncReportGuard guard(&report);
                copies = tu::run_sync(si, two_way ? &state : nullptr);
            }

            EXPECT_EQ(copies, 0u) << two_way << source_locked;
            EXPECT_TRUE(names(report, locked)) << two_way << source_locked;
            EXPECT_EQ(names_in(source_dir_), std::set<std::string>{"a.txt"}) << two_way << source_locked;
            EXPECT_EQ(names_in(target_dir_), std::set<std::string>{"a.txt"}) << two_way << source_locked;
            EXPECT_EQ(tu::run_sync(si, two_way ? &state : nullptr), 0u) << "unlocked, the files are equal: " << two_way << source_locked;
        }
}

TEST_F(LockedFileTest, LockedSourceIsNamedAndTheTargetStays)  // [LockedFileTests.md#LockedSourceIsNamedAndTheTargetStays]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    SyncReport report;
    std::size_t copies = 0;
    {
        const ExclusiveHandle lock(source_dir_ / "a.txt");
        ASSERT_TRUE(lock.held());
        SyncReportGuard guard(&report);
        copies = tu::run_sync(file_item(false));
    }

    EXPECT_EQ(copies, 0u);
    EXPECT_TRUE(names(report, source_dir_ / "a.txt"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
    EXPECT_EQ(names_in(target_dir_), std::set<std::string>{"a.txt"});
    EXPECT_EQ(tu::run_sync(file_item(false)), 1u) << "unlocked, the copy goes through";
}

TEST_F(LockedFileTest, LockedTargetIsNamedAndStays)  // [LockedFileTests.md#LockedTargetIsNamedAndStays]
{
    tu::write_text_file(source_dir_ / "a.txt", "neu");
    tu::write_text_file(target_dir_ / "a.txt", "alt", seconds(3600));
    SyncReport report;
    std::size_t copies = 0;
    {
        const ExclusiveHandle lock(target_dir_ / "a.txt");
        ASSERT_TRUE(lock.held());
        SyncReportGuard guard(&report);
        copies = tu::run_sync(file_item(false));
    }

    EXPECT_EQ(copies, 0u);
    EXPECT_TRUE(names(report, target_dir_ / "a.txt"));
    EXPECT_EQ(tu::read_text_file(target_dir_ / "a.txt"), "alt");
    EXPECT_EQ(names_in(target_dir_), std::set<std::string>{"a.txt"});
    EXPECT_EQ(tu::read_text_file(source_dir_ / "a.txt"), "neu");
}
#endif
