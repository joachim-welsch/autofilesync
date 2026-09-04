#include "pch.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "FileUtil.h"
#include "FilesystemUtil.h"
#include "SyncFileTests.h"
#include "Config.h"
#include "util.h"
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <Logging.h>

using namespace SyncFileTests;
using namespace std::chrono;
namespace fs = std::filesystem;

TEST(SyncFileTests, CheckSyncNotExistingFile)
{
	Synchronize::create(SyncType::Single);
    //Synchronize::instance().startLogging(LogLevel::Debug, L"C:\\temp\\test.log");
    LOG_DEBUG(L"~~~ Start test: CheckSyncNotExistingFile");
	SyncItem si;
    si.syncCycle(0);
	si.source(SourcePathName);
	si.target(TargetDir);
    tu::copyFile(ResourcePathName, SourcePathName);
    tu::eraseTestFile(TargetPathName);
	std::size_t copyCount = 0;
	SyncFile test(si, copyCount, false, true);
	Synchronize::start();
	test();
    EXPECT_EQ(copyCount, 1);
    EXPECT_TRUE(fs::exists(TargetPathName));
	std::filesystem::remove(TargetPathName);
    std::filesystem::remove(SourcePathName);
    LOG_DEBUG(L"~~~ End test: CheckSyncNotExistingFile");
}

TEST(SyncFileTests, CheckNotSyncExistingFile)
{
	//LOG_DEBUG(L"~~~ Start test: CheckNotSyncExistingFile");
	Synchronize::start();
	SyncItem si;
    si.syncCycle(0);
	si.source(SourcePathName);
	si.target(TargetDir);
    tu::copyFile(ResourcePathName, SourcePathName);
    tu::copyFile(ResourcePathName, TargetPathName);
	std::size_t copyCount = 0;
	SyncFile test(si, copyCount, false, true);
	test();
	EXPECT_EQ(copyCount, 0);
	std::filesystem::remove(TargetPathName);
    std::filesystem::remove(SourcePathName);
}

TEST(SyncFileTests, CheckSyncChangedSizeFile)
{
	//LOG_DEBUG(L"~~~ Start test: CheckSyncChangedSizeFile");
	// TODO CheckSyncChangedSizeFileSameSize
	Synchronize::start();
	SyncItem si;
    si.syncCycle(0);
	si.source(SourcePathName);
	si.target(TargetDir);
    CompareItems ci;
    ci.size(true);
    si.compareItems(ci);
    // Source must be the larger file: FileDifference only maps a size
    // difference to SourceModified (which copyToTarget acts on without extra
    // flags) when the source is bigger - a bigger destination means
    // DestModified instead, which needs copyWhenTargetNewer() to copy.
    tu::copyFile(LargerResourcePathName, SourcePathName);
    tu::copyFile(ResourcePathName, TargetPathName);
	std::size_t copyCount = 0;
	SyncFile test(si, copyCount, false, true);
	test();
	EXPECT_EQ(copyCount, 1);
	std::filesystem::remove(TargetPathName);
	std::filesystem::remove(TargetPathName +L".bak");
    std::filesystem::remove(SourcePathName);
}

TEST(SyncFileTests, CheckSyncChangedFileTime)
{
    //LOG_DEBUG(L"~~~ Start test: CheckSyncChangedFileTime");
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.source(SourcePathName);
    si.target(TargetDir);
    CompareItems ci;
	ci.time(true);
    si.compareItems(ci);
    tu::copyFile(ResourcePathName, SourcePathName);
    EXPECT_TRUE(tu::copySourceToDest(ResourcePathName, TargetPathName));
    EXPECT_TRUE(fsu::touch(SourcePathName));
    std::size_t copyCount = 0;
    SyncFile test(si, copyCount, false, true);
    test();
    EXPECT_EQ(copyCount, 1);
    std::filesystem::remove(SourcePathName);
    std::filesystem::remove(TargetPathName);
    std::filesystem::remove(TargetPathName + L".bak");
}

TEST(SyncFileTests, CheckSyncDoNotChangeTargetDirWithFile)
{
    //LOG_DEBUG(L"~~~ Start test: CheckSyncDoNotChangeTargetDirWithFile");
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.source(SourcePathName);
    si.target(TargetDir);
    CompareItems ci;
    ci.time(true);
    si.compareItems(ci);
    tu::copyFile(ResourcePathName, SourcePathName);
    EXPECT_TRUE(tu::copySourceToDest(ResourcePathName, TargetPathName));
    EXPECT_TRUE(fsu::touch(SourcePathName));
    std::size_t copyCount = 0;
    SyncFile test(si, copyCount, false, true);
    test();
    EXPECT_EQ(copyCount, 1);
    EXPECT_TRUE(fs::exists(TargetPathName));
    std::filesystem::remove(SourcePathName);
    std::filesystem::remove(TargetPathName);
    std::filesystem::remove(TargetPathName + L".bak");
}

// Regression test: the "count files" pre-pass (onlyCount=true) must behave like
// SyncBase::syncDirEntry() already does for Dir/Tree items - increment the
// counter without touching the filesystem. SyncFile::operator()() used to
// ignore onlyCount_ entirely and perform the real copy immediately, so a
// single-file SyncItem's "count" pass wasn't a dry run at all.
TEST(SyncFileTests, CheckOnlyCountDoesNotCopy)
{
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.source(SourcePathName);
    si.target(TargetDir);
    tu::copyFile(ResourcePathName, SourcePathName);
    tu::eraseTestFile(TargetPathName);
    std::size_t copyCount = 0;
    SyncFile test(si, copyCount, true, true); // onlyCount=true
    test();
    EXPECT_EQ(copyCount, 1); // still counted as "needs sync" ...
    EXPECT_FALSE(fs::exists(TargetPathName)); // ... but nothing was actually copied
    std::filesystem::remove(SourcePathName);
}
// Regression test (Fehler 1, round 4): saveCopyFile() used to "return true"
// for ANY SSH copy result before evaluating it, so a failed or aborted SSH
// transfer was reported as success - the pre-copy .bak rename was never rolled
// back (destination replaced by nothing/a truncated file) and callers went on
// to e.g. delete the source (delAftercopy). Only the content verification may
// be skipped for SSH, not the error handling.
TEST(SyncFileTests, FailedSshCopyRestoresBackupAndReportsFailure)
{
    Synchronize::start();
    // Account with an empty host: the connect fails immediately and locally,
    // keeping the test deterministic without network or DNS.
    SshAccount acc;
    acc.setName("afx_bad_ssh");
    acc.setHost("");
    acc.setUser("nobody");
    acc.setPassword("x");
    Config::instance().add(acc, true);

    fs::create_directories(TargetDir);
    const std::wstring dest = TargetDir + L"ssh_dest.txt";
    std::ofstream(util::wstring_to_string(dest)) << "original";
    const std::wstring source = L"afx_bad_ssh:/nonexistent/src.txt";

    SyncItem si;
    si.syncCycle(0);
    si.source(source); // parses to an SshPath because the account exists
    si.target(dest);
    std::size_t copyCount = 0;
    SyncFile sf(si, copyCount);
    EXPECT_FALSE(sf.testSaveCopyFile(source, dest));
    ASSERT_TRUE(fs::exists(dest)); // .bak rolled back over the destination
    {
        // Eigener Block: Windows verweigert das Loeschen einer noch geoeffneten
        // Datei, waehrend POSIX sie einfach unlinkt - ohne close() scheiterte
        // das fs::remove() unten mit "wird von einem anderen Prozess verwendet".
        std::ifstream in(util::wstring_to_string(dest));
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        EXPECT_EQ(content, "original");
    }
    EXPECT_FALSE(fs::exists(dest + L".bak"));

    std::filesystem::remove(dest);
    auto accounts = Config::instance().sshAccountsSnapshot();
    for (std::size_t i = 0; i < accounts.size(); ++i)
        if (accounts[i].name() == "afx_bad_ssh") { Config::instance().removeSshAccount(i); break; }
}

// Reaches the file-local helper in SyncFile.cpp; no header declares it. The
// default argument (call_wait = true) lives at the definition, so it must be
// passed explicitly here.
void set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait);

// Regression test (Fehler 3, round 4): set_last_write_time() retried in a
// "do { ... } while (ec)" loop with no exit condition. A persistent error
// (target drive unplugged, file deleted) spun the sync thread forever in
// 100ms sleeps; SyncControler::stopSync() then busy-waited on it from the
// GUI thread, freezing the application on shutdown. The retry loop must be
// bounded and give up on persistent errors.
TEST(SyncFileTests, SetLastWriteTimeGivesUpOnPersistentError)
{
    Synchronize::start(); // stop not requested: the bounded retry path itself must terminate
    const auto start = std::chrono::steady_clock::now();
    set_last_write_time(L"/nonexistent_afx_dir/nonexistent.txt", fs::file_time_type::clock::now(), false);
    const auto elapsed = duration_cast<seconds>(std::chrono::steady_clock::now() - start);
    EXPECT_LT(elapsed.count(), 10); // bounded (~1s with 10 x 100ms retries), not an endless loop
}

// Regression test (Fehler 10, round 5): copyDestToSource() called both
// set_last_write_time() (which already calls waitForUpdatedFiletime()
// internally by default) AND waitForUpdatedFiletime() again explicitly right
// after - doubling the wait for every two-way-sync back-copy (up to ~20s
// instead of ~10s in the worst case). copySourceToDest() has no such issue -
// it uses the plain util::set_last_write_time(), which never waits.
TEST(SyncFileTests, CopyDestToSourceDoesNotDoubleWaitForFiletime)
{
    Synchronize::start();
    const fs::path base = fs::temp_directory_path() / ("autofilesync_synctest_" + std::to_string(getpid()));
    fs::remove_all(base);
    fs::create_directories(base);
    const std::wstring srcPath = (base / "src.txt").wstring();
    const std::wstring destPath = (base / "dest.txt").wstring();
    std::ofstream(util::wstring_to_string(srcPath)) << "old content";
    std::ofstream(util::wstring_to_string(destPath)) << "new content";

    SyncItem si;
    si.syncCycle(0);
    si.source(srcPath);
    si.target(destPath);
    std::size_t copyCount = 0;
    SyncFile sf(si, copyCount);

    waitForUpdatedFiletimeCallCount = 0;
    sf.testCopyDestToSource(destPath, srcPath, 0);
    EXPECT_EQ(waitForUpdatedFiletimeCallCount.load(), 1);
    EXPECT_EQ(copyCount, 1u);

    fs::remove_all(base);
}

// Fehler 8 (round 6): SyncException passed a hardcoded "Fehler" to its
// runtime_error base instead of the real message, so every catch site
// logging ex.what() (there is no dedicated catch(SyncException&) anywhere -
// it's always caught via catch(std::exception&) or catch(...)) silently
// dropped the actual diagnostic moveFileToBackup() had built (which file,
// which underlying rename error).
TEST(SyncFileTests, SyncExceptionWhatReturnsTheRealMessage)
{
    SyncException ex(L"Testfehler: Umlaut äöü");
    EXPECT_STREQ(ex.what(), "Testfehler: Umlaut äöü");
    EXPECT_EQ(ex.get_message(), L"Testfehler: Umlaut äöü");
}

namespace {

// Legt eine Quelldatei der Groesse 0 an und raeumt Quelle wie Ziel auf.
class EmptyFileTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Synchronize::start();
        fs::create_directories(SourcePath);
        fs::create_directories(TargetDir);
        tu::eraseTestFile(SourcePathName);
        tu::eraseTestFile(TargetPathName);
        std::ofstream(util::wstring_to_string(SourcePathName)).close(); // 0 Bytes
        ASSERT_TRUE(fs::exists(SourcePathName));
        ASSERT_EQ(fs::file_size(SourcePathName), 0u);
    }

    void TearDown() override
    {
        tu::eraseTestFile(SourcePathName);
        tu::eraseTestFile(TargetPathName);
    }

    SyncItem item() const
    {
        SyncItem si;
        si.syncCycle(0);
        si.syncItemType(SyncItemType::File);
        si.source(SourcePathName);
        si.target(TargetPathName);
        return si;
    }
};

} // namespace

// Vorgabe: eine leere Quelldatei wird nicht kopiert. Groesse 0 ist haeufiger
// ein abgebrochener Schreibvorgang als eine gewollt leere Datei.
TEST_F(EmptyFileTest, EmptySourceFileIsNotCopiedByDefault)
{
    SyncItem si = item();
    ASSERT_FALSE(si.syncEmptyFiles());
    std::size_t copyCount = 0;
    SyncFile(si, copyCount)();

    EXPECT_FALSE(fs::exists(TargetPathName))
        << "ohne das Flag darf eine leere Quelldatei das Ziel nicht erreichen";
}

// Mit dem Flag wird sie kopiert - Platzhalterdateien (.gitkeep), Sperr- und
// Markierungsdateien sind genau dafuer da, leer zu sein.
TEST_F(EmptyFileTest, SyncEmptyFilesCopiesTheEmptySourceFile)
{
    SyncItem si = item();
    si.syncEmptyFiles(true);
    std::size_t copyCount = 0;
    SyncFile(si, copyCount)();

    ASSERT_TRUE(fs::exists(TargetPathName));
    EXPECT_EQ(fs::file_size(TargetPathName), 0u);
    EXPECT_EQ(copyCount, 1u) << "die kopierte Datei muss auch gezaehlt werden";
}

// Der gefaehrliche Fall: die Quelle ist leer, das Ziel nicht. Ohne das Flag
// bleibt der Zielinhalt erhalten - das ist der eigentliche Zweck der Vorgabe.
TEST_F(EmptyFileTest, EmptySourceDoesNotWipeANonEmptyTargetByDefault)
{
    std::ofstream(util::wstring_to_string(TargetPathName)) << "wertvoller Inhalt";
    SyncItem si = item();
    std::size_t copyCount = 0;
    SyncFile(si, copyCount)();

    ASSERT_TRUE(fs::exists(TargetPathName));
    EXPECT_GT(fs::file_size(TargetPathName), 0u)
        << "eine leere Quelle darf einen vorhandenen Zielinhalt nicht loeschen";
}

// Mit dem Flag spiegelt die Synchronisation die Quelle, auch wenn das heisst,
// dass ein vorhandener Zielinhalt durch eine leere Datei ersetzt wird. Das ist
// gewollt und hier festgehalten, damit die Tragweite des Flags dokumentiert ist.
TEST_F(EmptyFileTest, SyncEmptyFilesReplacesANonEmptyTarget)
{
    std::ofstream(util::wstring_to_string(TargetPathName)) << "wird ersetzt";
    SyncItem si = item();
    si.syncEmptyFiles(true);
    std::size_t copyCount = 0;
    SyncFile(si, copyCount)();

    ASSERT_TRUE(fs::exists(TargetPathName));
    EXPECT_EQ(fs::file_size(TargetPathName), 0u);
}

// Zaehlmodus fasst nichts an, auch nicht mit eingeschaltetem Flag.
TEST_F(EmptyFileTest, OnlyCountDoesNotCopyTheEmptyFile)
{
    SyncItem si = item();
    si.syncEmptyFiles(true);
    std::size_t copyCount = 0;
    SyncFile(si, copyCount, true)();

    EXPECT_EQ(copyCount, 1u);
    EXPECT_FALSE(fs::exists(TargetPathName));
}
