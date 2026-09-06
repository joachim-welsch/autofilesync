#include "pch.h"
#include "FileDifference.h"
#include "Config.h"
#include "SshAccount.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "SyncItem.h"
#include "util.h"
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
    fs::path testDir_;

    void SetUp() override
    {
        const std::string testName = testing::UnitTest::GetInstance()->current_test_info()->name();
        testDir_ = fs::temp_directory_path() / ("autofilesync_filediff_test_" + testName + "_" + std::to_string(getpid()));
        fs::remove_all(testDir_);
        fs::create_directories(testDir_);

        Config::instance().reset();
        SshAccount account;
        account.setName("unreachable");
        account.setUser("nobody");
        account.setHost("this-host-does-not-exist.invalid");
        account.setPassword("irrelevant");
        Config::instance().add(account);

        Synchronize::start(); // SyncFile::operator()() no-ops while Synchronize::stopRequested()
    }

    void TearDown() override
    {
        fs::remove_all(testDir_);
        Config::instance().reset();
    }
};

} // namespace

// Regression test for Fehler 2: readSshFileInfo() returns false on a
// connection/auth failure (invalid host, timeout, wrong credentials) without
// ever setting driveMounted=false, so FileDifference cannot tell "the SSH
// source is unreachable" apart from "the source file was genuinely deleted"
// (Result::SourceNotExits) - the latter is exactly what SyncFile treats as
// safe to act on when deleteInTarget() is enabled.
TEST_F(FileDifferenceSshTest, UnreachableSshSourceIsNotReportedAsSourceNotExists)
{
    const fs::path targetFile = testDir_ / "target.txt";
    std::ofstream(targetFile) << "target content";

    CompareItems ci;
    const std::wstring source = L"unreachable:/some/remote/file.txt";
    const std::wstring dest = targetFile.wstring();

    FileDifference fd(source, dest, ci, SSHPATH::source);
    fd();

    EXPECT_NE(fd.result(), FileDifference::Result::SourceNotExits)
        << "an unreachable SSH source must not be reported the same way as a deleted source file";
}

// Same defect, but asserting the actual user-visible consequence: with
// deleteInTarget enabled, a transient SSH failure on the source side must not
// delete the (perfectly intact) destination file.
TEST_F(FileDifferenceSshTest, UnreachableSshSourceDoesNotDeleteExistingTarget)
{
    const fs::path targetFile = testDir_ / "target.txt";
    std::ofstream(targetFile) << "target content";

    SyncItem si;
    si.syncCycle(0);
    si.deleteInTarget(true);
    si.source(L"unreachable:/some/remote/file.txt");
    si.target(targetFile.wstring());

    std::size_t copyCount = 0;
    SyncFile sync(si, copyCount, false, false);
    sync();

    EXPECT_TRUE(fs::exists(targetFile))
        << "target file was deleted because an unreachable SSH source was misreported as 'source deleted'";
}

namespace {

// Local-filesystem fixture for Fehler 5: content-only comparison cannot tell
// which side changed, only that they differ. With two-way sync, blindly
// reporting SourceModified silently discards a destination-only edit; the
// fix falls back to mtime as a tie-breaker, but only when twoWaySync is set,
// since one-way sync must keep always reporting SourceModified (SyncFile
// does not act on DestModified there unless copyWhenTargetNewer() is set,
// so anything else would stop the file from syncing at all).
class TwoWaySyncContentComparisonTest : public ::testing::Test {
protected:
    fs::path testDir_;
    fs::path sourceFile_;
    fs::path destFile_;

    void SetUp() override
    {
        const std::string testName = testing::UnitTest::GetInstance()->current_test_info()->name();
        testDir_ = fs::temp_directory_path() / ("autofilesync_twoway_test_" + testName + "_" + std::to_string(getpid()));
        fs::remove_all(testDir_);
        fs::create_directories(testDir_);
        sourceFile_ = testDir_ / "source.txt";
        destFile_ = testDir_ / "dest.txt";
        Synchronize::start(); // SyncFile::operator()() no-ops while Synchronize::stopRequested()
    }

    void TearDown() override
    {
        fs::remove_all(testDir_);
    }

    static void writeFileWithTime(const fs::path& path, const std::string& content, std::chrono::system_clock::time_point time)
    {
        std::ofstream(path) << content;
        const auto duration = time.time_since_epoch();
        const auto fsTime = fs::file_time_type(std::chrono::duration_cast<fs::file_time_type::duration>(duration));
        util::set_last_write_time(path.wstring(), fsTime, LogLevel::None);
    }
};

} // namespace

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPicksDestWhenDestIsNewerAndOnlyContentDiffers)
{
    const auto now = std::chrono::system_clock::now();
    writeFileWithTime(sourceFile_, "old content", now - std::chrono::hours(1));
    writeFileWithTime(destFile_, "new content", now);

    // FileDifference stores source/dest as references: they must outlive fd(),
    // so bind them to stable named locals rather than passing temporaries.
    const std::wstring source = sourceFile_.wstring();
    const std::wstring dest = destFile_.wstring();
    CompareItems ci; // default: content only
    FileDifference fd(source, dest, ci, SSHPATH::none, /*twoWaySync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::DestModified)
        << "the newer destination edit must not be reported as if the source had changed";
}

TEST_F(TwoWaySyncContentComparisonTest, TwoWaySyncPicksSourceWhenSourceIsNewerAndOnlyContentDiffers)
{
    const auto now = std::chrono::system_clock::now();
    writeFileWithTime(sourceFile_, "new content", now);
    writeFileWithTime(destFile_, "old content", now - std::chrono::hours(1));

    const std::wstring source = sourceFile_.wstring();
    const std::wstring dest = destFile_.wstring();
    CompareItems ci;
    FileDifference fd(source, dest, ci, SSHPATH::none, /*twoWaySync=*/true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceModified);
}

TEST_F(TwoWaySyncContentComparisonTest, OneWaySyncStillReportsSourceModifiedRegardlessOfTime)
{
    const auto now = std::chrono::system_clock::now();
    // Destination is newer, but this is one-way sync (twoWaySync defaults to
    // false): must keep reporting SourceModified, or SyncFile would stop
    // syncing this file entirely (see class comment above).
    writeFileWithTime(sourceFile_, "old content", now - std::chrono::hours(1));
    writeFileWithTime(destFile_, "new content", now);

    const std::wstring source = sourceFile_.wstring();
    const std::wstring dest = destFile_.wstring();
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
    writeFileWithTime(sourceFile_, "old content", now - std::chrono::hours(1));
    writeFileWithTime(destFile_, "new content", now);

    SyncItem si;
    si.syncCycle(0);
    si.twoWaySync(true);
    si.source(sourceFile_.wstring());
    si.target(destFile_.wstring());

    std::size_t copyCount = 0;
    SyncFile sync(si, copyCount, false, false);
    sync();

    std::ifstream sourceIn(sourceFile_);
    const std::string sourceContent((std::istreambuf_iterator<char>(sourceIn)), std::istreambuf_iterator<char>());
    std::ifstream destIn(destFile_);
    const std::string destContent((std::istreambuf_iterator<char>(destIn)), std::istreambuf_iterator<char>());

    EXPECT_EQ(sourceContent, "new content") << "the destination's newer edit should have been propagated back to the source";
    EXPECT_EQ(destContent, "new content") << "the destination itself must stay untouched";
}

// Fehler 1/round 7 follow-up: FileDifference's "else if" short-circuited the
// destination read entirely whenever the source was missing, so destSize()
// silently stayed at 0 for a SourceNotExits result - even though the
// destination file genuinely exists with real content. copyDestToSource()
// only fires when fd.destSize() > 0, so twoWaySync picking up a file that
// exists only in the destination silently did nothing (logged "Leere
// Zieldatei wurde nicht kopiert" for a file that was never actually empty).
TEST_F(TwoWaySyncContentComparisonTest, DestSizeIsReadEvenWhenSourceIsMissing)
{
    std::ofstream(destFile_) << "exists only in destination";

    // FileDifference stores source/dest as references - they must outlive
    // the object, so keep them as named locals rather than passing
    // sourceFile_.wstring()/destFile_.wstring() temporaries directly.
    const std::wstring source = sourceFile_.wstring();
    const std::wstring dest = destFile_.wstring();
    FileDifference fd(source, dest, CompareItems(), SSHPATH::none, true);
    fd();

    EXPECT_EQ(fd.result(), FileDifference::Result::SourceNotExits);
    EXPECT_GT(fd.destSize(), 0u);
}
