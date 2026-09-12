#include "pch.h"
#include "SyncDir.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncSshDirTests.h"
#include "Config.h"
#include "SshFileInspector.h"
#include "SshAccount.h"
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include "SshTest.h"
#include "FileUtil.h"

using namespace AFS_Core;

using namespace SSHTest;
using namespace tu;

namespace SyncSshDirTestConfig {
    const std::string SRC_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_src");
	const std::string DEST_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_dest");
    
    inline SshAccount create_test_account(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.set_name("test_dir_account");
        acc.set_user(user);
        acc.set_host(host);
        acc.set_password(password);
        return acc;
    }
}

class SyncSshDirTest : public ::testing::Test {
protected:
    void SetUp() override {
        namespace fs = std::filesystem;

        SKIP_WITHOUT_SSH_SERVER();

        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
        
        SshAccount acc;
        acc.set_name("localhost");
        acc.set_user(SSH_USER);
        acc.set_host(SSH_HOST);
        acc.set_password(SSH_PASSWORD);
        Config::instance().add(acc);

        static bool ssh_dest_cleaned = false;  // [SyncSshDirTests.md#remote-cleanup-once-per-process]
        if (!ssh_dest_cleaned) {
            tu::erase_ssh_test_dir_variants(
                L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()),
                { L"_empty", L"_many", L"_mixed", L"_overwrite" });
            ssh_dest_cleaned = true;
        }

        Synchronize::start();  // [SyncSshDirTests.md#reset-synchronize]
        }

    void TearDown() override {
        namespace fs = std::filesystem;
        
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
    }
};

TEST_F(SyncSshDirTest, SyncLocalDirToRemote)  // [SyncSshDirTests.md#sync-dir-copies-no-subdirs]
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR;
    std::ofstream(tu::concat_path(local_dir, "file1.txt")) << "content1";
    std::ofstream(tu::concat_path(local_dir, "file2.txt")) << "content2";
    std::ofstream(tu::concat_path(local_dir, "file3.txt")) << "content3";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo info1 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file1.txt");
    RemoteFileInfo info2 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file2.txt");
    RemoteFileInfo info3 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file3.txt");
    
    EXPECT_TRUE(info1.exists) << "file1.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info2.exists) << "file2.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info3.exists) << "file3.txt wurde nicht auf Remote gefunden";
    
    EXPECT_GT(info1.size_bytes, 0);
    EXPECT_GT(info2.size_bytes, 0);
    EXPECT_GT(info3.size_bytes, 0);
}

TEST_F(SyncSshDirTest, SyncRemoteDirToLocal)
{
    namespace fs = std::filesystem;

    fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
    fs::create_directories(SyncSshDirTestConfig::DEST_TEST_DIR);

    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "sshd_config")) << "test";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "other.txt")) << "other";

    {
        std::wstring seed_src_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
            SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/";
        std::wstring seed_dest_path = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
            SyncSshDirTestConfig::DEST_TEST_DIR.end());

        SyncItem seed;
        seed.sync_cycle(0);
        seed.sync_item_type(SyncItemType::Dir);
        seed.source(seed_src_path);
        seed.target(seed_dest_path);

        std::size_t seed_copy_count = 0;
        SyncDir seed_sync(seed, seed_copy_count, false);
        seed_sync();
    }

    std::wstring src_path = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    std::wstring dest_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
        SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/ssh_copy";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    EXPECT_TRUE(fs::exists(SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy"));

    std::string expected_file = SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy/sshd_config";
    EXPECT_TRUE(fs::exists(expected_file)) << "sshd_config wurde nicht kopiert";

    EXPECT_GT(copy_count, 0u) << "copyCount wurde bei SSH-Quelle nicht hochgezaehlt (Fortschrittsanzeige haengt)";  // [SyncSshDirTests.md#copy-count-regression]

    {  // [SyncSshDirTests.md#ssh-cleanup-needed]
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

TEST_F(SyncSshDirTest, SyncEmptyLocalDirToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/empty_dir";
    fs::create_directories(local_dir);
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_empty";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    
    EXPECT_NO_THROW(test());  // should throw nothing
}

TEST_F(SyncSshDirTest, SyncDirWithManyFiles)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/many_files";
    fs::create_directories(local_dir);
    
    const int file_count = 50;
    for (int i = 0; i < file_count; ++i) {
        std::ofstream(local_dir + "/file_" + std::to_string(i) + ".txt") 
            << "Content of file " << i;
    }
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_many";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo info0 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_0.txt");
    RemoteFileInfo info25 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_25.txt");
    RemoteFileInfo info49 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_49.txt");
    
    EXPECT_TRUE(info0.exists);
    EXPECT_TRUE(info25.exists);
    EXPECT_TRUE(info49.exists);
}

TEST_F(SyncSshDirTest, SyncDirectoryWithFiles)  // [SyncSshDirTests.md#sync-dir-copies-no-subdirs]
{
    namespace fs = std::filesystem;
    
    fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
    fs::create_directories(SyncSshDirTestConfig::DEST_TEST_DIR);

    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "root.txt")) << "root";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "data.txt")) << "data";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "config.txt")) << "config";
    
    std::wstring src_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(), SyncSshDirTestConfig::SRC_TEST_DIR.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    EXPECT_EQ(copy_count, 3);
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo root = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "root.txt"));
    RemoteFileInfo data = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "data.txt"));
    RemoteFileInfo config = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "config.txt"));
    
    EXPECT_TRUE(root.exists);
    EXPECT_TRUE(data.exists);
    EXPECT_TRUE(config.exists);

    {  // [SyncSshDirTests.md#ssh-cleanup-needed]
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

TEST_F(SyncSshDirTest, SyncDirWithDifferentFileTypes)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/mixed_types";
    fs::create_directories(local_dir);
    
    std::ofstream(local_dir + "/text.txt") << "Text file";
    std::ofstream(local_dir + "/script.sh") << "#!/bin/bash\necho 'test'";
    std::ofstream(local_dir + "/data.json") << "{\"key\": \"value\"}";
    std::ofstream(local_dir + "/config.xml") << "<?xml version=\"1.0\"?><root/>";
    
    std::ofstream bin_file(local_dir + "/binary.bin", std::ios::binary);
    std::vector<char> data(1024, 'B');
    bin_file.write(data.data(), data.size());
    bin_file.close();
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_mixed";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo txt = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/text.txt");
    RemoteFileInfo sh = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/script.sh");
    RemoteFileInfo bin = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/binary.bin");
    
    EXPECT_TRUE(txt.exists);
    EXPECT_TRUE(sh.exists);
    EXPECT_TRUE(bin.exists);
    EXPECT_EQ(bin.size_bytes, 1024);
}

TEST_F(SyncSshDirTest, SyncToExistingRemoteDir)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/overwrite_test";
    fs::create_directories(local_dir);
    std::ofstream(local_dir + "/initial.txt") << "initial content";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_overwrite";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test1(si, copy_count, false);
    test1();
    
    std::ofstream(local_dir + "/updated.txt") << "updated content";

    Synchronize::start();

    std::size_t copy_count2 = 0;
    SyncDir test2(si, copy_count2, false);
    test2();
    
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo initial = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/initial.txt");
    RemoteFileInfo updated = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/updated.txt");
    
    EXPECT_TRUE(initial.exists);
    EXPECT_TRUE(updated.exists);
}
