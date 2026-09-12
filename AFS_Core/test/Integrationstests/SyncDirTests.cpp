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
    std::wstring test_source_path = SOURCE_PATH;
    std::wstring test_target_path = TARGET_PATH;
    
    tu::erase_test_dir(test_source_path, false);
    tu::erase_test_dir(test_target_path, false);
    
    std::filesystem::create_directories(wstring_to_string(test_source_path) + "subdir");
    std::ofstream(wstring_to_string(test_source_path) + "file1.txt") << "content1";
    std::ofstream(wstring_to_string(test_source_path) + "file2.txt") << "content2";
    std::ofstream(wstring_to_string(test_source_path) + "subdir/file3.txt") << "content3";
    
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

TEST(SyncDirTests, SyncWithExistingTargetFiles)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directory(SOURCE_PATH);
    std::filesystem::create_directory(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/old_file.txt") << "alt";
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
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/new_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/old_file.txt"));  // [SyncDirTests.md#old-file-remains]
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

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
    EXPECT_NO_THROW(test());  // should throw nothing and do nothing either
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH)));
    EXPECT_TRUE(std::filesystem::is_empty(wstring_to_string(TARGET_PATH)));
    tu::erase_test_dir(TARGET_PATH, true);
}

TEST(SyncDirTests, SubdirectoryInSourceIsNeitherCopiedNorTreatedAsMissing)  // [SyncDirTests.md#SubdirectoryInSourceIsNeitherCopiedNorTreatedAsMissing]
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(SOURCE_PATH + L"subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/nested.txt") << "nested";
    std::filesystem::create_directories(TARGET_PATH);
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir") << "unrelated target file";  // [SyncDirTests.md#same-named-target-file]

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

TEST(SyncDirTests, DeleteInTargetRemovesFileMissingFromSource)  // [SyncDirTests.md#DeleteInTargetRemovesFileMissingFromSource]
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

TEST(SyncDirTests, TargetOnlyFileIsUntouchedWithoutDeleteInTargetOrTwoWaySync)  // [SyncDirTests.md#TargetOnlyFileIsUntouchedWithoutDeleteInTargetOrTwoWaySync]
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

TEST(SyncDirTests, DeleteInTargetSparesWildcardExcludedFiles)  // [SyncDirTests.md#DeleteInTargetSparesWildcardExcludedFiles]
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

TEST(SyncDirTests, TwoWaySyncCopiesTargetOnlyFileBackToSource)  // [SyncDirTests.md#TwoWaySyncCopiesTargetOnlyFileBackToSource]
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

TEST(SyncDirTests, ExcludedFileIsNotCopied)  // [SyncDirTests.md#ExcludedFileIsNotCopied]
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

TEST(SyncDirTests, DeleteInTargetSparesExcludedFiles)  // [SyncDirTests.md#DeleteInTargetSparesExcludedFiles]
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

TEST(SyncDirTests, SyncEmptyFilesCopiesEmptyFilesInADirectory)  // [SyncDirTests.md#SyncEmptyFilesCopiesEmptyFilesInADirectory]
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

TEST(SyncDirTests, EmptyFilesAreSkippedInADirectoryByDefault)  // [SyncDirTests.md#EmptyFilesAreSkippedInADirectoryByDefault]
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

TEST(SyncDirTests, DeleteInTargetKeepsTargetWhenSourceDirIsGone)  // [SyncDirTests.md#DeleteInTargetKeepsTargetWhenSourceDirIsGone]
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

class SyncDirUnreachableSshSourceTest : public ::testing::Test {  // [SyncDirTests.md#SyncDirUnreachableSshSourceTest]
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

TEST_F(SyncDirUnreachableSshSourceTest, DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable)  // [SyncDirTests.md#DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable]
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

TEST(SyncDirTests, SyncCreatesTargetDirWhoseParentIsMissing)  // [SyncDirTests.md#SyncCreatesTargetDirWhoseParentIsMissing]
{
    const std::wstring missing_parent = (std::filesystem::path(TestPaths::scratch_dir()) / L"missing_parent").wstring();  // [SyncDirTests.md#platform-separators]
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
