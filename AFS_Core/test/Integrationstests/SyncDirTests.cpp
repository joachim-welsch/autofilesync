#include "pch.h"
#include "SyncDir.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncDirTests.h"
#include "Config.h"
#include "SshAccount.h"
#include <Logging.h>
#include <StrUtil.h>

using namespace AFS_Core;

using namespace SyncDirTests;
using namespace util;
TEST(SyncDirTests, /*DISABLED_*/CheckSyncExistingFiles)
{
    // Create temporary test source directory (not using shared SourcePath)
    std::wstring test_source_path = SOURCE_PATH;
    std::wstring test_target_path = TARGET_PATH;
    
    tu::erase_test_dir(test_source_path, false);
    tu::erase_test_dir(test_target_path, false);
    
    // Create test structure
    std::filesystem::create_directories(wstring_to_string(test_source_path) + "subdir");
    std::ofstream(wstring_to_string(test_source_path) + "file1.txt") << "content1";
    std::ofstream(wstring_to_string(test_source_path) + "file2.txt") << "content2";
    std::ofstream(wstring_to_string(test_source_path) + "subdir/file3.txt") << "content3";
    
	//log_debug(L"~~~ Start test: CheckSyncExistingFiles");
	Synchronize::start();
	SyncItem si;
	si.sync_cycle(0);
	si.sync_item_type(SyncItemType::Dir);
	si.source(test_source_path);
	si.target(test_target_path);
    tu::erase_test_dir(test_target_path, false);
	std::size_t copy_count = 0;
	SyncDir test(si, copy_count, false);
	test();
	EXPECT_TRUE(std::filesystem::exists(test_target_path));
	EXPECT_TRUE(tu::check_copied_files(test_source_path, test_target_path));
    tu::erase_test_dir(test_source_path, true);
    tu::erase_test_dir(test_target_path, true);
}

// Test: Synchronisation, wenn Quellverzeichnis leer ist
TEST(SyncDirTests, SyncEmptySourceDir)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    std::filesystem::create_directory(SOURCE_PATH);
    tu::erase_test_dir(TARGET_PATH, false);
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH));
    EXPECT_TRUE(std::filesystem::is_empty(TARGET_PATH));
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation, wenn Zielverzeichnis bereits existiert und Dateien enthält
TEST(SyncDirTests, SyncWithExistingTargetFiles)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directory(SOURCE_PATH);
    std::filesystem::create_directory(TARGET_PATH);
    // Datei im Zielverzeichnis anlegen
    std::ofstream(wstring_to_string(TARGET_PATH) + "/old_file.txt") << "alt";
    // Datei im Quellverzeichnis anlegen
    std::ofstream(wstring_to_string(SOURCE_PATH) + "/new_file.txt") << "neu";
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    // Die neue Datei muss im Ziel sein
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/new_file.txt"));
    // Die alte Datei sollte noch existieren (je nach gewünschtem Verhalten ggf. anpassen)
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/old_file.txt"));
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation, wenn Quellverzeichnis nicht existiert
TEST(SyncDirTests, SyncNonExistentSourceDir)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    // Sollte keine Exception werfen, aber auch nichts tun
    EXPECT_NO_THROW(test());
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH)));
    EXPECT_TRUE(std::filesystem::is_empty(wstring_to_string(TARGET_PATH)));
    tu::erase_test_dir(TARGET_PATH, true);
}

// Fehler 2 (round 7): sync_dir_entry() passed subdirectories through to
// FileDifference unfiltered - fs::file_size() fails for a directory, which
// was misread as "source file not found." Harmless with delete_in_target off
// (confirmed here: the subdirectory is neither copied nor deleted), but with
// delete_in_target on it could delete an unrelated same-named *file* in the
// target. Verifies the subdirectory itself is left alone either way.
TEST(SyncDirTests, SubdirectoryInSourceIsNeitherCopiedNorTreatedAsMissing)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(SOURCE_PATH + L"subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/nested.txt") << "nested";
    std::filesystem::create_directories(TARGET_PATH);
    // Pre-existing target file that happens to share the subdirectory's name -
    // this must survive even with delete_in_target enabled.
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir") << "unrelated target file";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TARGET_PATH + L"/subdir/nested.txt")); // not recursed into
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/subdir")); // unrelated target file survives
    EXPECT_EQ(copy_count, 0u);

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Fehler 1 (round 7): SyncDir/SyncTree only ever enumerated the source side,
// so a file removed from the source was never noticed - delete_in_target
// ("Gelöschte Dateien im Ziel ebenfalls löschen") had no code path that could
// ever fire for Dir/Tree items, even though the dialog explicitly enables
// the checkbox for exactly these types. Verifies a target-only file (no
// counterpart in source) is now deleted when the option is enabled.
TEST(SyncDirTests, DeleteInTargetRemovesFileMissingFromSource)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/orphan.txt") << "no longer in source";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/still_there.txt") << "kept";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "still_there.txt") << "kept";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TARGET_PATH + L"/orphan.txt"));
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/still_there.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Without delete_in_target (and without two_way_sync), a target-only file must be
// left alone entirely - this is the existing, still-correct default.
TEST(SyncDirTests, TargetOnlyFileIsUntouchedWithoutDeleteInTargetOrTwoWaySync)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/onlyintarget.txt") << "keep me";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/onlyintarget.txt"));
    EXPECT_FALSE(std::filesystem::exists(SOURCE_PATH + L"onlyintarget.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// A wildcard-excluded target file must survive delete_in_target: it was never
// "managed" by this sync item in either direction, matching how the
// source-to-target pass already treats non-matching files as invisible.
TEST(SyncDirTests, DeleteInTargetSparesWildcardExcludedFiles)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/notes.log") << "not managed by *.txt";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.wildcard(L"*.txt");
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/notes.log"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// two_way_sync must copy a target-only file *back* to source instead of
// deleting it - the two options are mutually exclusive in effect per file
// (deleteTarget is checked first in SyncFile's decision logic).
TEST(SyncDirTests, TwoWaySyncCopiesTargetOnlyFileBackToSource)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/newintarget.txt") << "bring me back";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.two_way_sync(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(SOURCE_PATH + L"newintarget.txt"));
    std::ifstream in(wstring_to_string(SOURCE_PATH) + "newintarget.txt");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "bring me back");

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Exclusions feature: a file matching an exclusion pattern must never be
// synced, even though it matches an empty (match-everything) wildcard.
TEST(SyncDirTests, ExcludedFileIsNotCopied)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(SOURCE_PATH) + "keep.txt") << "keep";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "cache.tmp") << "exclude me";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    si.exclusions(exclusions);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/keep.txt"));
    EXPECT_FALSE(std::filesystem::exists(TARGET_PATH + L"/cache.tmp"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// An excluded file must also survive delete_in_target: it is not managed by
// this sync item in either direction, so a pre-existing target-side copy
// (from before the exclusion was added, say) must not get deleted either.
TEST(SyncDirTests, DeleteInTargetSparesExcludedFiles)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/cache.tmp") << "not managed";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    si.exclusions(exclusions);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/cache.tmp"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Auf Verzeichnisebene muss das Flag genauso greifen: SyncDir reicht jede
// Datei einzeln an SyncFile weiter, die leeren duerfen dabei nicht als
// Sonderfall verlorengehen - und die nicht leeren muessen unabhaengig davon
// weiter kopiert werden.
TEST(SyncDirTests, SyncEmptyFilesCopiesEmptyFilesInADirectory)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::ofstream(wstring_to_string(SOURCE_PATH) + "leer.txt").close();
    std::ofstream(wstring_to_string(SOURCE_PATH) + "voll.txt") << "Inhalt";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.sync_empty_files(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(TARGET_PATH + L"/leer.txt"));
    EXPECT_EQ(std::filesystem::file_size(TARGET_PATH + L"/leer.txt"), 0u);
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/voll.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Ohne das Flag bleibt die leere Datei liegen, die gefuellte wird kopiert.
TEST(SyncDirTests, EmptyFilesAreSkippedInADirectoryByDefault)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::ofstream(wstring_to_string(SOURCE_PATH) + "leer.txt").close();
    std::ofstream(wstring_to_string(SOURCE_PATH) + "voll.txt") << "Inhalt";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(TARGET_PATH + L"/leer.txt"));
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/voll.txt"))
        << "das Ueberspringen darf nur die leere Datei betreffen";

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// The worst case for this option: the source is not "empty", it is *gone*
// (drive unplugged, network share not mounted, directory removed). Every
// target file then looks like an orphan, so a naive target-only pass would
// wipe the entire backup. Only the directory_iterator throwing before
// sync_target_only_entries() is reached prevents that - assert it stays that way.
TEST(SyncDirTests, DeleteInTargetKeepsTargetWhenSourceDirIsGone)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup1.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup2.txt") << "irreplaceable";
    ASSERT_FALSE(std::filesystem::exists(SOURCE_PATH));

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/backup1.txt"))
        << "an unavailable source directory must never be read as 'everything was deleted'";
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/backup2.txt"));
    EXPECT_EQ(copy_count, 0u);

    tu::erase_test_dir(TARGET_PATH, true);
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
        account.set_name("unreachable");
        account.set_user("nobody");
        account.set_host("this-host-does-not-exist.invalid");
        account.set_password("irrelevant");
        Config::instance().add(account);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        tu::erase_test_dir(TARGET_PATH, true);
    }
};

} // namespace

// The SSH counterpart of the test above, and a nastier one: ssh_list_dir()
// returns an empty vector on a connection or authentication failure, which is
// indistinguishable from "the remote directory is empty" - so the target-only
// pass does run, over every file in the target. What saves the data is the
// per-file check in FileDifference (a failed SSH read reports "drive gone",
// not SourceNotExits). This test pins that second line of defence down at the
// SyncDir level, where the mass deletion would actually happen.
TEST_F(SyncDirUnreachableSshSourceTest, DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable)
{
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup1.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup2.txt") << "irreplaceable";

    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(L"unreachable:/remote/backup");
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    ASSERT_TRUE(si.ssh_source());
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/backup1.txt"))
        << "an unreachable SSH source lists as empty - the target must not be emptied along with it";
    EXPECT_TRUE(std::filesystem::exists(TARGET_PATH + L"/backup2.txt"));
}

// The target directory used to be created with create_directory(), so a target
// whose parent did not exist yet - a fresh backup drive, a path one level
// deeper than anything present - failed on every run and the item never synced.
TEST(SyncDirTests, SyncCreatesTargetDirWhoseParentIsMissing)
{
    // Built through fs::path so the separators are the platform's own.
    const std::wstring missing_parent = (std::filesystem::path(TestPaths::scratch_dir()) / L"missing_parent").wstring();
    const std::wstring target_path = (std::filesystem::path(missing_parent) / L"dir").wstring();
    tu::erase_test_dir(missing_parent, false);
    tu::erase_test_dir(SOURCE_PATH, false);
    std::filesystem::create_directories(SOURCE_PATH);
    std::ofstream(wstring_to_string(SOURCE_PATH) + "file1.txt") << "content1";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(SOURCE_PATH);
    si.target(target_path);
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(target_path));
    EXPECT_TRUE(tu::check_copied_files(SOURCE_PATH, target_path));
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(missing_parent, true);
}
