#include "pch.h"
#include "FileDifference.h"
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

// Registers an SSH account whose host name can never resolve (RFC 2606
// reserves the ".invalid" TLD for exactly this), so every connection attempt
// fails fast and deterministically without depending on a real SSH server -
// this exercises the same failure path as a genuine "SSH-Verbindungsfehler"
// (Fehler 2) portably, in any environment.
class FileDifferenceSshTest : public ::testing::Test {
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

// Regression test for Fehler 2: read_ssh_file_info() returns false on a
// connection/auth failure (invalid host, timeout, wrong credentials) without
// ever setting drive_mounted=false, so FileDifference cannot tell "the SSH
// source is unreachable" apart from "the source file was genuinely deleted"
// (Result::SourceNotExits) - the latter is exactly what SyncFile treats as
// safe to act on when delete_in_target() is enabled.
TEST_F(FileDifferenceSshTest, UnreachableSshSourceIsNotReportedAsSourceNotExists)
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

// Same defect, but asserting the actual user-visible consequence: with
// delete_in_target enabled, a transient SSH failure on the source side must not
// delete the (perfectly intact) destination file.
TEST_F(FileDifferenceSshTest, UnreachableSshSourceDoesNotDeleteExistingTarget)
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

// Local-filesystem fixture for Fehler 5: content-only comparison cannot tell
// which side changed, only that they differ. With two-way sync, blindly
// reporting SourceModified silently discards a destination-only edit; the
// fix falls back to mtime as a tie-breaker, but only when two_way_sync is set,
// since one-way sync must keep always reporting SourceModified (SyncFile
// does not act on DestModified there unless copy_when_target_newer() is set,
// so anything else would stop the file from syncing at all).
class TwoWaySyncContentComparisonTest : public ::testing::Test {
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

    // FileDifference stores source/dest as references: they must outlive fd(),
    // so bind them to stable named locals rather than passing temporaries.
    const std::wstring source = source_file_.wstring();
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

TEST_F(TwoWaySyncContentComparisonTest, OneWaySyncStillReportsSourceModifiedRegardlessOfTime)
{
    const auto now = std::chrono::system_clock::now();
    // Destination is newer, but this is one-way sync (two_way_sync defaults to
    // false): must keep reporting SourceModified, or SyncFile would stop
    // syncing this file entirely (see class comment above).
    write_file_with_time(source_file_, "old content", now - std::chrono::hours(1));
    write_file_with_time(dest_file_, "new content", now);

    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

// End-to-end version of the first test: with the destination genuinely newer,
// two-way sync must copy destination -> source, preserving the edit, instead
// of silently overwriting the destination with the older source content.
TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPreservesNewerDestinationEditInsteadOfOverwritingIt)
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

// Fehler 1/round 7 follow-up: FileDifference's "else if" short-circuited the
// destination read entirely whenever the source was missing, so dest_size()
// silently stayed at 0 for a SourceNotExits result - even though the
// destination file genuinely exists with real content. copy_dest_to_source()
// only fires when fd.dest_size() > 0, so two_way_sync picking up a file that
// exists only in the destination silently did nothing (logged "Leere
// Zieldatei wurde nicht kopiert" for a file that was never actually empty).
TEST_F(TwoWaySyncContentComparisonTest, DestSizeIsReadEvenWhenSourceIsMissing)
{
    std::ofstream(dest_file_) << "exists only in destination";

    // FileDifference stores source/dest as references - they must outlive
    // the object, so keep them as named locals rather than passing
    // source_file_.wstring()/dest_file_.wstring() temporaries directly.
    const std::wstring source = source_file_.wstring();
    const std::wstring dest = dest_file_.wstring();
    FileDifference fd(source, dest, CompareItems(), SSHPATH::none, true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceNotExits);
    EXPECT_GT(fd.dest_size(), 0u);
}
