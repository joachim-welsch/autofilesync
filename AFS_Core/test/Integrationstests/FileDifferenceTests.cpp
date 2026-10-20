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
#include "FileDifference.h"
#include "FileUtil.h"
#include "Config.h"
#include "SshAccount.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "SyncItem.h"
#include "Util.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <chrono>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class FileDifferenceSshTest : public ::testing::Test {  // [FileDifferenceTests.md#FileDifferenceSshTest]
protected:
    fs::path test_dir_;

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_dir_ = fs::temp_directory_path() / ("autofilesync_filediff_test_" + test_name + "_" + std::to_string(getpid()));
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);

        Config::instance().reset();
        SshAccount account;
        account.set_name("unreachable");
        account.set_user("nobody");
        account.set_host("this-host-does-not-exist.invalid");
        account.set_password("irrelevant");
        Config::instance().add(account);

        Synchronize::start(); // SyncFile::operator()() no-ops while Synchronize::stopping()
    }

    void TearDown() override
    {
        fs::remove_all(test_dir_);
        Config::instance().reset();
    }
};

} // namespace

TEST_F(FileDifferenceSshTest, UnreachableSshSourceIsNotReportedAsSourceNotExists)  // [FileDifferenceTests.md#UnreachableSshSourceIsNotReportedAsSourceNotExists]
{
    const fs::path target_file = test_dir_ / "target.txt";
    std::ofstream(target_file) << "target content";

    CompareItems ci;
    const std::wstring source = L"unreachable:/some/remote/file.txt";
    const std::wstring dest = target_file.wstring();

    FileDifference fd(source, dest, ci, SSHPATH::source);
    fd();

    EXPECT_NE(fd.result(), FileDifference::Result::SourceNotExits)
        << "an unreachable SSH source must not be reported the same way as a deleted source file";
}

TEST_F(FileDifferenceSshTest, UnreachableSshSourceDoesNotDeleteExistingTarget)  // [FileDifferenceTests.md#UnreachableSshSourceDoesNotDeleteExistingTarget]
{
    const fs::path target_file = test_dir_ / "target.txt";
    std::ofstream(target_file) << "target content";

    SyncItem si;
    si.sync_cycle(0);
    si.delete_in_target(true);
    si.source(L"unreachable:/some/remote/file.txt");
    si.target(target_file.wstring());

    std::size_t copy_count = 0;
    SyncFile sync(si, copy_count, false, false);
    sync();

    EXPECT_TRUE(fs::exists(target_file))
        << "target file was deleted because an unreachable SSH source was misreported as 'source deleted'";
}

namespace {

class TwoWaySyncContentComparisonTest : public ::testing::Test {  // [FileDifferenceTests.md#TwoWaySyncContentComparisonTest]
protected:
    fs::path test_dir_;
    fs::path source_file_;
    fs::path dest_file_;

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_dir_ = fs::temp_directory_path() / ("autofilesync_twoway_test_" + test_name + "_" + std::to_string(getpid()));
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);
        source_file_ = test_dir_ / "source.txt";
        dest_file_ = test_dir_ / "dest.txt";
        Synchronize::start(); // SyncFile::operator()() no-ops while Synchronize::stopping()
    }

    void TearDown() override
    {
        fs::remove_all(test_dir_);
    }

    static void write_file_with_time(const fs::path& path, const std::string& content, std::chrono::system_clock::time_point time)
    {
        std::ofstream(path) << content;
        const auto duration = time.time_since_epoch();
        const auto fs_time = fs::file_time_type(std::chrono::duration_cast<fs::file_time_type::duration>(duration));
        util::set_last_write_time(path.wstring(), fs_time, LogLevel::None);
    }
};

} // namespace

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPicksDestWhenDestIsNewerAndOnlyContentDiffers)
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "old content", now - std::chrono::hours(1));
    write_file_with_time(dest_file_, "new content", now);

    const std::wstring source = source_file_.wstring();  // [FileDifferenceTests.md#references-must-outlive]
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci; // default: content only
    FileDifference fd(source, dest, ci, SSHPATH::none, /*two_way_sync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::DestModified)
        << "the newer destination edit must not be reported as if the source had changed";
}

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPicksSourceWhenSourceIsNewerAndOnlyContentDiffers)
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "new content", now);
    write_file_with_time(dest_file_, "old content", now - std::chrono::hours(1));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none, /*two_way_sync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncReportsAConflictWhenOnlyTheContentDiffers)  // [FileDifferenceTests.md#TwoWaySyncReportsAConflictWhenOnlyTheContentDiffers]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "content A", now);
    write_file_with_time(dest_file_, "content B", now);

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none, /*two_way_sync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::ContentConflict);
}

TEST_F(TwoWaySyncContentComparisonTest, OneWaySyncKnowsNoContentConflict)  // [FileDifferenceTests.md#OneWaySyncKnowsNoContentConflict]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "content A", now);
    write_file_with_time(dest_file_, "content B", now);

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, OneWaySyncStillReportsSourceModifiedRegardlessOfTime)
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "old content", now - std::chrono::hours(1));  // [FileDifferenceTests.md#one-way-keeps-sourcemodified]
    write_file_with_time(dest_file_, "new content", now);

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPreservesNewerDestinationEditInsteadOfOverwritingIt)  // [FileDifferenceTests.md#TwoWaySyncPreservesNewerDestinationEditInsteadOfOverwritingIt]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "old content", now - std::chrono::hours(1));
    write_file_with_time(dest_file_, "new content", now);

    SyncItem si;
    si.sync_cycle(0);
    si.two_way_sync(true);
    si.source(source_file_.wstring());
    si.target(dest_file_.wstring());

    std::size_t copy_count = 0;
    SyncFile sync(si, copy_count, false, false);
    sync();

    std::ifstream source_in(source_file_);
    const std::string source_content((std::istreambuf_iterator<char>(source_in)), std::istreambuf_iterator<char>());
    std::ifstream dest_in(dest_file_);
    const std::string dest_content((std::istreambuf_iterator<char>(dest_in)), std::istreambuf_iterator<char>());

    EXPECT_EQ(source_content, "new content") << "the destination's newer edit should have been propagated back to the source";
    EXPECT_EQ(dest_content, "new content") << "the destination itself must stay untouched";
}

TEST_F(TwoWaySyncContentComparisonTest, DestSizeIsReadEvenWhenSourceIsMissing)  // [FileDifferenceTests.md#DestSizeIsReadEvenWhenSourceIsMissing]
{
    std::ofstream(dest_file_) << "exists only in destination";

    const std::wstring source = source_file_.wstring();  // [FileDifferenceTests.md#references-must-outlive]
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, CompareItems(), SSHPATH::none, true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceNotExits);
    EXPECT_GT(fd.dest_size(), 0u);
}

namespace {

CompareItems time_only(uint16_t tolerance_seconds)
{
    CompareItems ci;
    ci.content(false);
    ci.time(true);
    ci.time_tolerance_seconds(tolerance_seconds);
    return ci;
}

}

TEST_F(TwoWaySyncContentComparisonTest, SourceNewerWithinTheToleranceCountsAsEqual)  // [FileDifferenceTests.md#time-tolerance]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(dest_file_, "same", now);
    write_file_with_time(source_file_, "same", now + std::chrono::seconds(1));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, time_only(DEFAULT_TIME_TOLERANCE_SECONDS));
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::Equal) << "a difference inside the tolerance must not look like a change";
}

TEST_F(TwoWaySyncContentComparisonTest, SourceNewerBeyondTheToleranceIsSourceModified)
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(dest_file_, "same", now);
    write_file_with_time(source_file_, "same", now + std::chrono::seconds(3));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, time_only(DEFAULT_TIME_TOLERANCE_SECONDS));
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, DestNewerWithinTheToleranceCountsAsEqual)
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "same", now);
    write_file_with_time(dest_file_, "same", now + std::chrono::seconds(1));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, time_only(DEFAULT_TIME_TOLERANCE_SECONDS));
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::Equal);
}

TEST_F(TwoWaySyncContentComparisonTest, ZeroToleranceSeesASubSecondDifference)  // [FileDifferenceTests.md#time-tolerance]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(dest_file_, "same", now);
    write_file_with_time(source_file_, "same", now + std::chrono::milliseconds(100));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, time_only(0));
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncTreatsTimesWithinTheToleranceAsAContentConflict)  // [FileDifferenceTests.md#time-tolerance]
{
    const auto now = std::chrono::system_clock::now();
    write_file_with_time(source_file_, "AAAA", now);
    write_file_with_time(dest_file_, "BBBB", now + std::chrono::seconds(1));

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none, /*two_way_sync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::ContentConflict)
        << "a second between the write times is no evidence for either side being the newer one";
}

namespace {
class ThreeWayTest: public ::testing::Test {  // [FileDifferenceTests.md#ThreeWayTest]
protected:
    using Result = FileDifference::Result;
    const fs::path dir_ = fs::temp_directory_path() / "autofilesync_threeway_test";
    const fs::path source_ = dir_ / "source.txt";
    const fs::path dest_ = dir_ / "dest.txt";
    const std::wstring source_w_ = source_.wstring();
    const std::wstring dest_w_ = dest_.wstring();
    FileState base_;

    void SetUp() override
    {
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        Synchronize::start();
        tu::write_text_file(source_, "v1", std::chrono::seconds(3600));
        tu::write_text_file(dest_, "v1", std::chrono::seconds(3600));
        base_ = {fs::file_size(source_), fs::last_write_time(source_), fs::last_write_time(dest_)};
    }
    void TearDown() override { fs::remove_all(dir_); }

    Result result(const FileState* base) const
    {
        CompareItems ci;
        ci.content(false);
        ci.time(true);
        ci.size(true);
        FileDifference fd(source_w_, dest_w_, ci, SSHPATH::none, true, base);
        fd();
        return fd.result();
    }
    Result with_base() const { return result(&base_); }
    Result without_base() const { return result(nullptr); }
};
}

TEST_F(ThreeWayTest, UnchangedSidesAreEqual)
{
    EXPECT_EQ(with_base(), Result::Equal);
}

TEST_F(ThreeWayTest, MissingSourceIsADeletionOnlyWhileTheTargetIsUnchanged)
{
    fs::remove(source_);
    EXPECT_EQ(with_base(), Result::SourceDeleted);
    EXPECT_EQ(without_base(), Result::SourceNotExits);
    tu::write_text_file(dest_, "edited on the target");
    EXPECT_EQ(with_base(), Result::SourceNotExits) << "an edit since the last pass must be copied back, not deleted";
}

TEST_F(ThreeWayTest, MissingTargetIsADeletionOnlyWhileTheSourceIsUnchanged)
{
    fs::remove(dest_);
    EXPECT_EQ(with_base(), Result::DestDeleted);
    EXPECT_EQ(without_base(), Result::DestNotExists);
    tu::write_text_file(source_, "edited on the source");
    EXPECT_EQ(with_base(), Result::DestNotExists);
}

TEST_F(ThreeWayTest, BothSidesMissingIsNeitherExists)
{
    fs::remove(source_);
    fs::remove(dest_);
    EXPECT_EQ(with_base(), Result::NeitherExists);
    EXPECT_EQ(without_base(), Result::NeitherExists);
}

TEST_F(ThreeWayTest, TheChangedSideIsTheModifiedOneWhateverTheTimesSay)  // [FileDifferenceTests.md#TheChangedSideIsTheModifiedOneWhateverTheTimesSay]
{
    tu::write_text_file(source_, "older write time but changed", std::chrono::seconds(7200));
    EXPECT_EQ(with_base(), Result::SourceModified);
    EXPECT_EQ(without_base(), Result::DestModified);
}

TEST_F(ThreeWayTest, ChangedTargetIsDestModified)
{
    tu::write_text_file(dest_, "changed on the target", std::chrono::seconds(7200));
    EXPECT_EQ(with_base(), Result::DestModified);
    EXPECT_EQ(without_base(), Result::SourceModified);
}

TEST_F(ThreeWayTest, BothChangedIsAConflictUnlessTheyAgree)  // [FileDifferenceTests.md#BothChangedIsAConflictUnlessTheyAgree]
{
    tu::write_text_file(source_, "source v2");
    tu::write_text_file(dest_, "target version 2");
    EXPECT_EQ(with_base(), Result::BothModified);
    tu::write_text_file(source_, "the same v2", std::chrono::seconds(60));
    tu::write_text_file(dest_, "the same v2", std::chrono::seconds(60));
    EXPECT_EQ(with_base(), Result::Equal);
}

TEST_F(ThreeWayTest, SizeAloneMarksASideAsChanged)
{
    tu::write_text_file(source_, "v1!");
    util::set_last_write_time(source_w_, base_.source_time);
    EXPECT_EQ(with_base(), Result::SourceModified);
    EXPECT_EQ(without_base(), Result::SourceModified);
}
