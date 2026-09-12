#include "pch.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "FileUtil.h"
#include "FilesystemUtil.h"
#include "SyncFileTests.h"
#include "Config.h"
#include "Util.h"
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <Logging.h>

using namespace AFS_Core;

using namespace SyncFileTests;
using namespace std::chrono;
namespace fs = std::filesystem;

TEST(SyncFileTests, CheckSyncNotExistingFile)
{
	Synchronize::create(SyncType::Single);
    log_debug(L"~~~ Start test: CheckSyncNotExistingFile");
	SyncItem si;
    si.sync_cycle(0);
	si.source(SOURCE_PATH_NAME);
	si.target(TARGET_DIR);
    tu::copy_file(RESOURCE_PATH_NAME, SOURCE_PATH_NAME);
    fs::create_directories(TARGET_DIR);  // [SyncFileTests.md#target-dir-must-exist]
    tu::erase_test_file(TARGET_PATH_NAME);
	std::size_t copy_count = 0;
	SyncFile test(si, copy_count, false, true);
	Synchronize::start();
	test();
    EXPECT_EQ(copy_count, 1);
    EXPECT_TRUE(fs::exists(TARGET_PATH_NAME));
	std::filesystem::remove(TARGET_PATH_NAME);
    std::filesystem::remove(SOURCE_PATH_NAME);
    log_debug(L"~~~ End test: CheckSyncNotExistingFile");
}

TEST(SyncFileTests, CheckNotSyncExistingFile)
{
	Synchronize::start();
	SyncItem si;
    si.sync_cycle(0);
	si.source(SOURCE_PATH_NAME);
	si.target(TARGET_DIR);
    tu::copy_file(RESOURCE_PATH_NAME, SOURCE_PATH_NAME);
    tu::copy_file(RESOURCE_PATH_NAME, TARGET_PATH_NAME);
	std::size_t copy_count = 0;
	SyncFile test(si, copy_count, false, true);
	test();
	EXPECT_EQ(copy_count, 0);
	std::filesystem::remove(TARGET_PATH_NAME);
    std::filesystem::remove(SOURCE_PATH_NAME);
}

TEST(SyncFileTests, CheckSyncChangedSizeFile)
{
	Synchronize::start();  // TODO CheckSyncChangedSizeFileSameSize
	SyncItem si;
    si.sync_cycle(0);
	si.source(SOURCE_PATH_NAME);
	si.target(TARGET_DIR);
    CompareItems ci;
    ci.size(true);
    si.compare_items(ci);
    tu::copy_file(LARGER_RESOURCE_PATH_NAME, SOURCE_PATH_NAME);  // [SyncFileTests.md#source-must-be-larger]
    tu::copy_file(RESOURCE_PATH_NAME, TARGET_PATH_NAME);
	std::size_t copy_count = 0;
	SyncFile test(si, copy_count, false, true);
	test();
	EXPECT_EQ(copy_count, 1);
	std::filesystem::remove(TARGET_PATH_NAME);
	std::filesystem::remove(TARGET_PATH_NAME +L".bak");
    std::filesystem::remove(SOURCE_PATH_NAME);
}

TEST(SyncFileTests, CheckSyncChangedFileTime)
{
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.source(SOURCE_PATH_NAME);
    si.target(TARGET_DIR);
    CompareItems ci;
	ci.time(true);
    si.compare_items(ci);
    tu::copy_file(RESOURCE_PATH_NAME, SOURCE_PATH_NAME);
    EXPECT_TRUE(tu::copy_source_to_dest(RESOURCE_PATH_NAME, TARGET_PATH_NAME));
    EXPECT_TRUE(fsu::touch(SOURCE_PATH_NAME));
    std::size_t copy_count = 0;
    SyncFile test(si, copy_count, false, true);
    test();
    EXPECT_EQ(copy_count, 1);
    std::filesystem::remove(SOURCE_PATH_NAME);
    std::filesystem::remove(TARGET_PATH_NAME);
    std::filesystem::remove(TARGET_PATH_NAME + L".bak");
}

TEST(SyncFileTests, CheckSyncDoNotChangeTargetDirWithFile)
{
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.source(SOURCE_PATH_NAME);
    si.target(TARGET_DIR);
    CompareItems ci;
    ci.time(true);
    si.compare_items(ci);
    tu::copy_file(RESOURCE_PATH_NAME, SOURCE_PATH_NAME);
    EXPECT_TRUE(tu::copy_source_to_dest(RESOURCE_PATH_NAME, TARGET_PATH_NAME));
    EXPECT_TRUE(fsu::touch(SOURCE_PATH_NAME));
    std::size_t copy_count = 0;
    SyncFile test(si, copy_count, false, true);
    test();
    EXPECT_EQ(copy_count, 1);
    EXPECT_TRUE(fs::exists(TARGET_PATH_NAME));
    std::filesystem::remove(SOURCE_PATH_NAME);
    std::filesystem::remove(TARGET_PATH_NAME);
    std::filesystem::remove(TARGET_PATH_NAME + L".bak");
}

TEST(SyncFileTests, CheckOnlyCountDoesNotCopy)  // [SyncFileTests.md#CheckOnlyCountDoesNotCopy]
{
    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.source(SOURCE_PATH_NAME);
    si.target(TARGET_DIR);
    tu::copy_file(RESOURCE_PATH_NAME, SOURCE_PATH_NAME);
    tu::erase_test_file(TARGET_PATH_NAME);
    std::size_t copy_count = 0;
    SyncFile test(si, copy_count, true, true); // onlyCount=true
    test();
    EXPECT_EQ(copy_count, 1); // still counted as "needs sync" ...
    EXPECT_FALSE(fs::exists(TARGET_PATH_NAME)); // ... but nothing was actually copied
    std::filesystem::remove(SOURCE_PATH_NAME);
}
TEST(SyncFileTests, FailedSshCopyRestoresBackupAndReportsFailure)  // [SyncFileTests.md#FailedSshCopyRestoresBackupAndReportsFailure]
{
    Synchronize::start();
    SshAccount acc;  // [SyncFileTests.md#empty-host-fails-locally]
    acc.set_name("afx_bad_ssh");
    acc.set_host("");
    acc.set_user("nobody");
    acc.set_password("x");
    Config::instance().add(acc, true);

    fs::create_directories(TARGET_DIR);
    const std::wstring dest = TARGET_DIR + L"ssh_dest.txt";
    std::ofstream(util::wstring_to_string(dest)) << "original";
    const std::wstring source = L"afx_bad_ssh:/nonexistent/src.txt";

    SyncItem si;
    si.sync_cycle(0);
    si.source(source); // parses to an SshPath because the account exists
    si.target(dest);
    std::size_t copy_count = 0;
    SyncFile sf(si, copy_count);
    EXPECT_FALSE(sf.test_save_copy_file(source, dest));
    ASSERT_TRUE(fs::exists(dest)); // .bak rolled back over the destination
    {
        std::ifstream in(util::wstring_to_string(dest));  // [SyncFileTests.md#own-block-for-windows]
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        EXPECT_EQ(content, "original");
    }
    EXPECT_FALSE(fs::exists(dest + L".bak"));

    std::filesystem::remove(dest);
    auto accounts = Config::instance().ssh_accounts_snapshot();
    for (std::size_t i = 0; i < accounts.size(); ++i)
        if (accounts[i].name() == "afx_bad_ssh") { Config::instance().remove_ssh_account(i); break; }
}

namespace AFS_Core { void set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait); }  // [SyncFileTests.md#set_last_write_time]

TEST(SyncFileTests, SetLastWriteTimeGivesUpOnPersistentError)  // [SyncFileTests.md#SetLastWriteTimeGivesUpOnPersistentError]
{
    Synchronize::start(); // stop not requested: the bounded retry path itself must terminate
    const auto start = std::chrono::steady_clock::now();
    set_last_write_time(L"/nonexistent_afx_dir/nonexistent.txt", fs::file_time_type::clock::now(), false);
    const auto elapsed = duration_cast<seconds>(std::chrono::steady_clock::now() - start);
    EXPECT_LT(elapsed.count(), 10); // bounded (~1s with 10 x 100ms retries), not an endless loop
}

TEST(SyncFileTests, CopyDestToSourceDoesNotDoubleWaitForFiletime)  // [SyncFileTests.md#CopyDestToSourceDoesNotDoubleWaitForFiletime]
{
    Synchronize::start();
    const fs::path base = fs::temp_directory_path() / ("autofilesync_synctest_" + std::to_string(getpid()));
    fs::remove_all(base);
    fs::create_directories(base);
    const std::wstring src_path = (base / "src.txt").wstring();
    const std::wstring dest_path = (base / "dest.txt").wstring();
    std::ofstream(util::wstring_to_string(src_path)) << "old content";
    std::ofstream(util::wstring_to_string(dest_path)) << "new content";

    SyncItem si;
    si.sync_cycle(0);
    si.source(src_path);
    si.target(dest_path);
    std::size_t copy_count = 0;
    SyncFile sf(si, copy_count);

    wait_for_updated_filetime_call_count = 0;
    sf.test_copy_dest_to_source(dest_path, src_path, 0);
    EXPECT_EQ(wait_for_updated_filetime_call_count.load(), 1);
    EXPECT_EQ(copy_count, 1u);

    fs::remove_all(base);
}

TEST(SyncFileTests, SyncExceptionWhatReturnsTheRealMessage)  // [SyncFileTests.md#SyncExceptionWhatReturnsTheRealMessage]
{
    SyncException ex(L"Testfehler: Umlaut äöü");
    EXPECT_STREQ(ex.what(), "Testfehler: Umlaut äöü");
    EXPECT_EQ(ex.get_message(), L"Testfehler: Umlaut äöü");
}

namespace {

class EmptyFileTest : public ::testing::Test {  // [SyncFileTests.md#EmptyFileTest]
protected:
    void SetUp() override
    {
        Synchronize::start();
        fs::create_directories(SOURCE_PATH);
        fs::create_directories(TARGET_DIR);
        tu::erase_test_file(SOURCE_PATH_NAME);
        tu::erase_test_file(TARGET_PATH_NAME);
        std::ofstream(util::wstring_to_string(SOURCE_PATH_NAME)).close(); // 0 Bytes
        ASSERT_TRUE(fs::exists(SOURCE_PATH_NAME));
        ASSERT_EQ(fs::file_size(SOURCE_PATH_NAME), 0u);
    }

    void TearDown() override
    {
        tu::erase_test_file(SOURCE_PATH_NAME);
        tu::erase_test_file(TARGET_PATH_NAME);
    }

    SyncItem item() const
    {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source(SOURCE_PATH_NAME);
        si.target(TARGET_PATH_NAME);
        return si;
    }
};

} // namespace

TEST_F(EmptyFileTest, EmptySourceFileIsNotCopiedByDefault)  // [SyncFileTests.md#EmptySourceFileIsNotCopiedByDefault]
{
    SyncItem si = item();
    ASSERT_FALSE(si.sync_empty_files());
    std::size_t copy_count = 0;
    SyncFile(si, copy_count)();

    EXPECT_FALSE(fs::exists(TARGET_PATH_NAME))
        << "ohne das Flag darf eine leere Quelldatei das Ziel nicht erreichen";
}

TEST_F(EmptyFileTest, SyncEmptyFilesCopiesTheEmptySourceFile)  // [SyncFileTests.md#SyncEmptyFilesCopiesTheEmptySourceFile]
{
    SyncItem si = item();
    si.sync_empty_files(true);
    std::size_t copy_count = 0;
    SyncFile(si, copy_count)();

    ASSERT_TRUE(fs::exists(TARGET_PATH_NAME));
    EXPECT_EQ(fs::file_size(TARGET_PATH_NAME), 0u);
    EXPECT_EQ(copy_count, 1u) << "die kopierte Datei muss auch gezaehlt werden";
}

TEST_F(EmptyFileTest, EmptySourceDoesNotWipeANonEmptyTargetByDefault)  // [SyncFileTests.md#EmptySourceDoesNotWipeANonEmptyTargetByDefault]
{
    std::ofstream(util::wstring_to_string(TARGET_PATH_NAME)) << "wertvoller Inhalt";
    SyncItem si = item();
    std::size_t copy_count = 0;
    SyncFile(si, copy_count)();

    ASSERT_TRUE(fs::exists(TARGET_PATH_NAME));
    EXPECT_GT(fs::file_size(TARGET_PATH_NAME), 0u)
        << "eine leere Quelle darf einen vorhandenen Zielinhalt nicht loeschen";
}

TEST_F(EmptyFileTest, SyncEmptyFilesReplacesANonEmptyTarget)  // [SyncFileTests.md#SyncEmptyFilesReplacesANonEmptyTarget]
{
    std::ofstream(util::wstring_to_string(TARGET_PATH_NAME)) << "wird ersetzt";
    SyncItem si = item();
    si.sync_empty_files(true);
    std::size_t copy_count = 0;
    SyncFile(si, copy_count)();

    ASSERT_TRUE(fs::exists(TARGET_PATH_NAME));
    EXPECT_EQ(fs::file_size(TARGET_PATH_NAME), 0u);
}

TEST_F(EmptyFileTest, OnlyCountDoesNotCopyTheEmptyFile)  // [SyncFileTests.md#OnlyCountDoesNotCopyTheEmptyFile]
{
    SyncItem si = item();
    si.sync_empty_files(true);
    std::size_t copy_count = 0;
    SyncFile(si, copy_count, true)();

    EXPECT_EQ(copy_count, 1u);
    EXPECT_FALSE(fs::exists(TARGET_PATH_NAME));
}
