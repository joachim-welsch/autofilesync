#include "pch.h"
#include "SyncDir.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncDirTests.h"
#include "Config.h"
#include "SshAccount.h"
#include <Logging.h>
#include <strutil.h>

using namespace SyncDirTests;
using namespace util;
TEST(SyncDirTests, /*DISABLED_*/CheckSyncExistingFiles)
{
    // Create temporary test source directory (not using shared SourcePath)
    std::wstring testSourcePath = SourcePath;
    std::wstring testTargetPath = TargetPath;
    
    tu::eraseTestDir(testSourcePath, false);
    tu::eraseTestDir(testTargetPath, false);
    
    // Create test structure
    std::filesystem::create_directories(wstring_to_string(testSourcePath) + "subdir");
    std::ofstream(wstring_to_string(testSourcePath) + "file1.txt") << "content1";
    std::ofstream(wstring_to_string(testSourcePath) + "file2.txt") << "content2";
    std::ofstream(wstring_to_string(testSourcePath) + "subdir/file3.txt") << "content3";
    
	//LOG_DEBUG(L"~~~ Start test: CheckSyncExistingFiles");
	Synchronize::start();
	SyncItem si;
	si.syncCycle(0);
	si.syncItemType(SyncItemType::Dir);
	si.source(testSourcePath);
	si.target(testTargetPath);
    tu::eraseTestDir(testTargetPath, false);
	std::size_t copyCount = 0;
	SyncDir test(si, copyCount, false);
	test();
	EXPECT_TRUE(std::filesystem::exists(testTargetPath));
	EXPECT_TRUE(tu::checkCopiedFiles(testSourcePath, testTargetPath));
    tu::eraseTestDir(testSourcePath, true);
    tu::eraseTestDir(testTargetPath, true);
}

// Test: Synchronisation, wenn Quellverzeichnis leer ist
TEST(SyncDirTests, SyncEmptySourceDir)
{
    tu::eraseTestDir(SourcePath, true);
    std::filesystem::create_directory(SourcePath);
    tu::eraseTestDir(TargetPath, false);
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    EXPECT_TRUE(std::filesystem::exists(TargetPath));
    EXPECT_TRUE(std::filesystem::is_empty(TargetPath));
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation, wenn Zielverzeichnis bereits existiert und Dateien enthält
TEST(SyncDirTests, SyncWithExistingTargetFiles)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directory(SourcePath);
    std::filesystem::create_directory(TargetPath);
    // Datei im Zielverzeichnis anlegen
    std::ofstream(wstring_to_string(TargetPath) + "/old_file.txt") << "alt";
    // Datei im Quellverzeichnis anlegen
    std::ofstream(wstring_to_string(SourcePath) + "/new_file.txt") << "neu";
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    // Die neue Datei muss im Ziel sein
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/new_file.txt"));
    // Die alte Datei sollte noch existieren (je nach gewünschtem Verhalten ggf. anpassen)
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/old_file.txt"));
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation, wenn Quellverzeichnis nicht existiert
TEST(SyncDirTests, SyncNonExistentSourceDir)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    // Sollte keine Exception werfen, aber auch nichts tun
    EXPECT_NO_THROW(test());
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath)));
    EXPECT_TRUE(std::filesystem::is_empty(wstring_to_string(TargetPath)));
    tu::eraseTestDir(TargetPath, true);
}

// Fehler 2 (round 7): syncDirEntry() passed subdirectories through to
// FileDifference unfiltered - fs::file_size() fails for a directory, which
// was misread as "source file not found." Harmless with deleteInTarget off
// (confirmed here: the subdirectory is neither copied nor deleted), but with
// deleteInTarget on it could delete an unrelated same-named *file* in the
// target. Verifies the subdirectory itself is left alone either way.
TEST(SyncDirTests, SubdirectoryInSourceIsNeitherCopiedNorTreatedAsMissing)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(SourcePath + L"subdir");
    std::ofstream(wstring_to_string(SourcePath) + "subdir/nested.txt") << "nested";
    std::filesystem::create_directories(TargetPath);
    // Pre-existing target file that happens to share the subdirectory's name -
    // this must survive even with deleteInTarget enabled.
    std::ofstream(wstring_to_string(TargetPath) + "/subdir") << "unrelated target file";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TargetPath + L"/subdir/nested.txt")); // not recursed into
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/subdir")); // unrelated target file survives
    EXPECT_EQ(copyCount, 0u);

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Fehler 1 (round 7): SyncDir/SyncTree only ever enumerated the source side,
// so a file removed from the source was never noticed - deleteInTarget
// ("Gelöschte Dateien im Ziel ebenfalls löschen") had no code path that could
// ever fire for Dir/Tree items, even though the dialog explicitly enables
// the checkbox for exactly these types. Verifies a target-only file (no
// counterpart in source) is now deleted when the option is enabled.
TEST(SyncDirTests, DeleteInTargetRemovesFileMissingFromSource)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/orphan.txt") << "no longer in source";
    std::ofstream(wstring_to_string(TargetPath) + "/still_there.txt") << "kept";
    std::ofstream(wstring_to_string(SourcePath) + "still_there.txt") << "kept";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TargetPath + L"/orphan.txt"));
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/still_there.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Without deleteInTarget (and without twoWaySync), a target-only file must be
// left alone entirely - this is the existing, still-correct default.
TEST(SyncDirTests, TargetOnlyFileIsUntouchedWithoutDeleteInTargetOrTwoWaySync)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/onlyintarget.txt") << "keep me";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/onlyintarget.txt"));
    EXPECT_FALSE(std::filesystem::exists(SourcePath + L"onlyintarget.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// A wildcard-excluded target file must survive deleteInTarget: it was never
// "managed" by this sync item in either direction, matching how the
// source-to-target pass already treats non-matching files as invisible.
TEST(SyncDirTests, DeleteInTargetSparesWildcardExcludedFiles)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/notes.log") << "not managed by *.txt";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.wildcard(L"*.txt");
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/notes.log"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// twoWaySync must copy a target-only file *back* to source instead of
// deleting it - the two options are mutually exclusive in effect per file
// (deleteTarget is checked first in SyncFile's decision logic).
TEST(SyncDirTests, TwoWaySyncCopiesTargetOnlyFileBackToSource)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/newintarget.txt") << "bring me back";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.twoWaySync(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(SourcePath + L"newintarget.txt"));
    std::ifstream in(wstring_to_string(SourcePath) + "newintarget.txt");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "bring me back");

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Exclusions feature: a file matching an exclusion pattern must never be
// synced, even though it matches an empty (match-everything) wildcard.
TEST(SyncDirTests, ExcludedFileIsNotCopied)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(SourcePath) + "keep.txt") << "keep";
    std::ofstream(wstring_to_string(SourcePath) + "cache.tmp") << "exclude me";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    si.exclusions(exclusions);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/keep.txt"));
    EXPECT_FALSE(std::filesystem::exists(TargetPath + L"/cache.tmp"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// An excluded file must also survive deleteInTarget: it is not managed by
// this sync item in either direction, so a pre-existing target-side copy
// (from before the exclusion was added, say) must not get deleted either.
TEST(SyncDirTests, DeleteInTargetSparesExcludedFiles)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/cache.tmp") << "not managed";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    si.exclusions(exclusions);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/cache.tmp"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Auf Verzeichnisebene muss das Flag genauso greifen: SyncDir reicht jede
// Datei einzeln an SyncFile weiter, die leeren duerfen dabei nicht als
// Sonderfall verlorengehen - und die nicht leeren muessen unabhaengig davon
// weiter kopiert werden.
TEST(SyncDirTests, SyncEmptyFilesCopiesEmptyFilesInADirectory)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::ofstream(wstring_to_string(SourcePath) + "leer.txt").close();
    std::ofstream(wstring_to_string(SourcePath) + "voll.txt") << "Inhalt";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.syncEmptyFiles(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(TargetPath + L"/leer.txt"));
    EXPECT_EQ(std::filesystem::file_size(TargetPath + L"/leer.txt"), 0u);
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/voll.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Ohne das Flag bleibt die leere Datei liegen, die gefuellte wird kopiert.
TEST(SyncDirTests, EmptyFilesAreSkippedInADirectoryByDefault)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::ofstream(wstring_to_string(SourcePath) + "leer.txt").close();
    std::ofstream(wstring_to_string(SourcePath) + "voll.txt") << "Inhalt";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TargetPath + L"/leer.txt"));
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/voll.txt"))
        << "das Ueberspringen darf nur die leere Datei betreffen";

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// The worst case for this option: the source is not "empty", it is *gone*
// (drive unplugged, network share not mounted, directory removed). Every
// target file then looks like an orphan, so a naive target-only pass would
// wipe the entire backup. Only the directory_iterator throwing before
// syncTargetOnlyEntries() is reached prevents that - assert it stays that way.
TEST(SyncDirTests, DeleteInTargetKeepsTargetWhenSourceDirIsGone)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/backup1.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TargetPath) + "/backup2.txt") << "irreplaceable";
    ASSERT_FALSE(std::filesystem::exists(SourcePath));

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/backup1.txt"))
        << "an unavailable source directory must never be read as 'everything was deleted'";
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/backup2.txt"));
    EXPECT_EQ(copyCount, 0u);

    tu::eraseTestDir(TargetPath, true);
}

namespace {

// Registers an SSH account whose host can never resolve (RFC 2606 reserves
// ".invalid"), so the connection fails fast and deterministically without a
// real server - see the identical fixture in FileDifferenceTests.cpp.
class SyncDirUnreachableSshSourceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Config::instance().reset();
        SshAccount account;
        account.setName("unreachable");
        account.setUser("nobody");
        account.setHost("this-host-does-not-exist.invalid");
        account.setPassword("irrelevant");
        Config::instance().add(account);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        tu::eraseTestDir(TargetPath, true);
    }
};

} // namespace

// The SSH counterpart of the test above, and a nastier one: sshListDir()
// returns an empty vector on a connection or authentication failure, which is
// indistinguishable from "the remote directory is empty" - so the target-only
// pass does run, over every file in the target. What saves the data is the
// per-file check in FileDifference (a failed SSH read reports "drive gone",
// not SourceNotExits). This test pins that second line of defence down at the
// SyncDir level, where the mass deletion would actually happen.
TEST_F(SyncDirUnreachableSshSourceTest, DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable)
{
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(TargetPath);
    std::ofstream(wstring_to_string(TargetPath) + "/backup1.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TargetPath) + "/backup2.txt") << "irreplaceable";

    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(L"unreachable:/remote/backup");
    si.target(TargetPath);
    si.deleteInTarget(true);
    ASSERT_TRUE(si.sshSource());
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/backup1.txt"))
        << "an unreachable SSH source lists as empty - the target must not be emptied along with it";
    EXPECT_TRUE(std::filesystem::exists(TargetPath + L"/backup2.txt"));
}

// The target directory used to be created with create_directory(), so a target
// whose parent did not exist yet - a fresh backup drive, a path one level
// deeper than anything present - failed on every run and the item never synced.
TEST(SyncDirTests, SyncCreatesTargetDirWhoseParentIsMissing)
{
    // Built through fs::path so the separators are the platform's own.
    const std::wstring missingParent = (std::filesystem::path(TestPaths::scratchDir()) / L"missing_parent").wstring();
    const std::wstring targetPath = (std::filesystem::path(missingParent) / L"dir").wstring();
    tu::eraseTestDir(missingParent, false);
    tu::eraseTestDir(SourcePath, false);
    std::filesystem::create_directories(SourcePath);
    std::ofstream(wstring_to_string(SourcePath) + "file1.txt") << "content1";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(SourcePath);
    si.target(targetPath);
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(targetPath));
    EXPECT_TRUE(tu::checkCopiedFiles(SourcePath, targetPath));
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(missingParent, true);
}
