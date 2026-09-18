#include "pch.h"
#include "SshFileInspector.h"
#include "SshAccount.h"
#include "SyncFileSsh.h"
#include "Config.h"
#include "Synchronize.h"
#include "SyncFile.h"
#include "SyncSshFileTests.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <FilesystemUtil.h>
#include "FileUtil.h"
#include "DirUtil.h"
#include "StrUtil.h"
#include "SshTest.h"
#include "DirUtil.h"

using namespace AFS_Core;

using namespace SSHTest;
using namespace util;
using std::string;

namespace SyncSshFileTestConfig {
#ifdef _UNIX
    const std::string TEST_REMOTE_PATH = fsu::home_dir() + "/tmp/test_file.txt";
    const std::string TEST_REMOTE_DIR = fsu::home_dir() + "/tmp";
#else
    const std::string TEST_REMOTE_PATH = fsu::home_dir() + "\\tmp\\test_file.txt";
    const std::string TEST_REMOTE_DIR = fsu::home_dir() + "\\tmp";
#endif
    inline SshAccount create_test_account(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.set_name("test_account");
        acc.set_user(user);
        acc.set_host(host);
        acc.set_password(password);
        return acc;
    }
}

class SyncSshFileInspectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        SKIP_WITHOUT_SSH_SERVER();  // [SyncSshFileTests.md#SyncSshFileInspectorTest]
    }

    void TearDown() override {
    }
};

TEST_F(SyncSshFileInspectorTest, ConstructorConnectsSuccessfully)
{
    EXPECT_NO_THROW({
        SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());
    });
}

TEST_F(SyncSshFileInspectorTest, ConstructorFailsWithWrongPassword)
{
    EXPECT_THROW({
        SshFileInspector inspector(
            SyncSshFileTestConfig::create_test_account(SSH_USER, SSH_HOST, "wrong_password")
        );
    }, std::runtime_error);
}

TEST_F(SyncSshFileInspectorTest, ConstructorFailsWithInvalidHost)
{
    EXPECT_THROW({
        SshFileInspector inspector(
            SyncSshFileTestConfig::create_test_account(SSH_USER, "invalid.host.that.does.not.exist.123", SSH_PASSWORD)
        );
    }, std::runtime_error);
}

TEST_F(SyncSshFileInspectorTest, InspectExistingFile)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);  // assumption: /etc/hosts exists on most Unix systems
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.size_bytes, 0);
    EXPECT_NE(info.last_modified_iso, "N/A");
    EXPECT_GT(info.raw_mtime, 0);
}

TEST_F(SyncSshFileInspectorTest, InspectNonExistingFile)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    std::string non_existing = tu::concat_ssh_temp("this_file_absolutely_does_not_exist_") +  // [SyncSshFileTests.md#ssh-temp-path]
                                std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    RemoteFileInfo info = inspector.inspect_file(non_existing);
    
    EXPECT_FALSE(info.exists);
    EXPECT_EQ(info.size_bytes, 0);
    EXPECT_EQ(info.last_modified_iso, "N/A");
    EXPECT_EQ(info.raw_mtime, 0);
}

TEST_F(SyncSshFileInspectorTest, InspectMultipleFiles)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    std::vector<std::string> test_files = {
        tu::concat_path(ETC_PATH, "hosts"),
#if defined(_MACOS)
        tu::concat_path(ETC_PATH, "services"),  // macOS keeps no /etc/hostname - the name lives in the system configuration
        tu::concat_path(ETC_PATH, "passwd")
#elif defined(_UNIX)
        tu::concat_path(ETC_PATH, "hostname"),
        tu::concat_path(ETC_PATH, "passwd")
#else
        tu::concat_path(ETC_PATH, "networks"),
        tu::concat_path(ETC_PATH, "protocol")
#endif
    };

    for (const auto& file : test_files) {
        RemoteFileInfo info = inspector.inspect_file(file);
        EXPECT_TRUE(info.exists) << "File should exist: " << file;
        EXPECT_GT(info.size_bytes, 0) << "File should have size > 0: " << file;
    }
}

TEST_F(SyncSshFileInspectorTest, CheckFileSize)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);  // /etc/hosts should be small (< 10KB on most systems)
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.size_bytes, 0);
    EXPECT_LT(info.size_bytes, 10240); // < 10KB
}

TEST_F(SyncSshFileInspectorTest, CheckTimestampFormat)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);
    
    EXPECT_TRUE(info.exists);
    EXPECT_NE(info.last_modified_iso, "N/A");
    
    EXPECT_GE(info.last_modified_iso.length(), 19);  // at least "YYYY-MM-DD HH:MM:SS", possibly followed by a time zone
    EXPECT_EQ(info.last_modified_iso[4], '-');
    EXPECT_EQ(info.last_modified_iso[7], '-');
    EXPECT_EQ(info.last_modified_iso[10], ' ');
    EXPECT_EQ(info.last_modified_iso[13], ':');
    EXPECT_EQ(info.last_modified_iso[16], ':');
}

TEST_F(SyncSshFileInspectorTest, InspectEmptyPath)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file("");
    
    EXPECT_FALSE(info.exists);
}

TEST_F(SyncSshFileInspectorTest, DestructorCleansUpProperly)
{
    for (int i = 0; i < 3; ++i) {  // several instances in a row must work
        EXPECT_NO_THROW({
            SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());
            RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);
            EXPECT_TRUE(info.exists);
        });
    }
}

TEST_F(SyncSshFileInspectorTest, TimestampConsistency)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.raw_mtime, 0);
    
    time_t min_time = 1577836800; // 2020-01-01
    time_t max_time = 1893456000; // 2030-01-01
    
    EXPECT_GT(info.raw_mtime, min_time);
    EXPECT_LT(info.raw_mtime, max_time);
}

TEST_F(SyncSshFileInspectorTest, InspectDirectory)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(ETC_PATH);
    
    if (info.exists) {  // a directory may be reported as existing, depending on the SFTP implementation
        EXPECT_GE(info.size_bytes, 0);
    }
}

TEST_F(SyncSshFileInspectorTest, StressTestMultipleQueries)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    for (int i = 0; i < 100; ++i) {
        RemoteFileInfo info = inspector.inspect_file(HOSTS_PATH);
        EXPECT_TRUE(info.exists);
    }
}

TEST_F(SyncSshFileInspectorTest, InspectPathWithSpaces)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(tu::concat_ssh_temp("file with spaces.txt"));  // the path may well not exist, but nothing may crash
    
    EXPECT_FALSE(info.exists);
}

TEST_F(SyncSshFileInspectorTest, InspectRelativePath)
{
    SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());

    RemoteFileInfo info = inspector.inspect_file(string("..")+ HOSTS_PATH);  // relative paths may or may not work, depending on the SFTP implementation
    EXPECT_GE(info.size_bytes, 0);
}

class SyncSshFileCopyTest : public ::testing::Test {

protected:
    const std::string TEST_FILENAME = "ssh_copy_test_file.txt";
    const std::string TEST_CONTENT = "Dies ist ein Test für SSH-Kopieren vom Laptop!";
    const std::string LOCAL_TEST_DIR = (std::filesystem::temp_directory_path() / "ssh_copy_test").string();  // [SyncSshFileTests.md#local-test-dir]

    void SetUp() override {
        SKIP_WITHOUT_SSH_SERVER();
        namespace fs = std::filesystem;
        fs::create_directories(LOCAL_TEST_DIR);

        std::string local_file_path = tu::concat_path(LOCAL_TEST_DIR, TEST_FILENAME);
        std::ofstream out_file(local_file_path);
        out_file << TEST_CONTENT;
        out_file.close();

        Synchronize::start();  // otherwise the copy is aborted as "Stopped"
    }

    void TearDown() override {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::remove_all(LOCAL_TEST_DIR, ec);
    }
    
    SshAccount create_laptop_account() {
        return SyncSshFileTestConfig::create_test_account();
    }
    
    void ensure_account_in_config() {
        SshAccount acc;  // [SyncSshFileTests.md#account-name-must-be-localhost]
        acc.set_name("localhost");
        acc.set_user(SSH_USER);
        acc.set_host(SSH_HOST);
        acc.set_password(SSH_PASSWORD);
        Config::instance().add(acc);
    }
};

TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemote)
{
    ensure_account_in_config();
    namespace fs = std::filesystem;

    fs::create_directories(tu::test_source_dir());
    fs::permissions(fs::path(tu::test_source_dir()).parent_path(), fs::perms::all);  // [SyncSshFileTests.md#shared-parent-permissions]
    const std::string TEST_FILENAME = tu::concat_path(tu::test_source_dir(), "data.txt");
    std::ofstream(TEST_FILENAME) << "data";

    const std::string test_remote_filename = "localhost:" + tu::concat_path(tu::test_dest_dir(), "data.txt");;
    const std::wstring remote_dest_parent = L"localhost:" + string_to_wstring(fs::path(tu::test_dest_dir()).parent_path().string());  // [SyncSshFileTests.md#target-dir-must-exist]
    const std::wstring remote_dest_dir = L"localhost:" + string_to_wstring(tu::test_dest_dir());
    if (!ssh_exists(remote_dest_parent))
        ssh_mkdir(remote_dest_parent);
    if (!ssh_exists(remote_dest_dir))
        ssh_mkdir(remote_dest_dir);

    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(string_to_wstring(TEST_FILENAME));
    si.target(string_to_wstring(test_remote_filename));

    std::size_t copy_count = 0;
    SyncFile test(si, copy_count, false);
    test();

    EXPECT_EQ(copy_count, 1);
    SshFileInspector inspector(create_laptop_account());
    RemoteFileInfo info = inspector.inspect_file(get_remote_path(string_to_wstring(test_remote_filename)));  // [SyncSshFileTests.md#inspect-path-without-prefix]

    EXPECT_TRUE(info.exists) << "Remote-Datei wurde nicht gefunden";
    EXPECT_GT(info.size_bytes, 0) << "Remote-Datei ist leer";
    std::filesystem::remove(string_to_wstring(TEST_FILENAME));
    ssh_remove_file(string_to_wstring(test_remote_filename));  // [SyncSshFileTests.md#ssh-delete-needed]
}

TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemoteTmp)
{
    ensure_account_in_config();
    
    const std::string TEST_FILENAME = "ssh_copy_test_file.txt";
    const std::string test_remote_filename = "ssh_copy_test_file_remote.txt";

    std::string src_path = tu::concat_temp(tu::concat_path("ssh_copy_test", TEST_FILENAME));
    std::string dest_path =  "localhost:" + tu::concat_ssh_temp(test_remote_filename);

    std::filesystem::create_directories(std::filesystem::path(src_path).parent_path());  // [SyncSshFileTests.md#write-to-src-path-directly]
    std::ofstream(src_path) << TEST_CONTENT;

    CopyResult result = copy_fs_to_ssh(string_to_wstring(src_path), string_to_wstring(dest_path));

    EXPECT_EQ(result, CopyResult::OK) << "Kopieren von lokal nach SSH fehlgeschlagen";

    SshFileInspector inspector(create_laptop_account());
    RemoteFileInfo info = inspector.inspect_file(tu::concat_ssh_temp(test_remote_filename));

    EXPECT_TRUE(info.exists) << "Remote-Datei wurde nicht gefunden";
    EXPECT_GT(info.size_bytes, 0) << "Remote-Datei ist leer";

    std::filesystem::remove(src_path);
    ssh_remove_file(string_to_wstring(dest_path));
}

TEST_F(SyncSshFileCopyTest, CopyRemoteFileToLocal)
{
    ensure_account_in_config();
    
#ifdef _UNIX  // create the remote file first, from /etc/hosts which exists
    std::wstring src_path = L"localhost:/etc/hosts";
#else
    std::wstring src_path = L"localhost:C:\\Windows\\System32\\drivers\\etc\\hosts";
#endif
    std::wstring dest_path = tu::concat_temp(tu::concat_path(L"ssh_copy_test", L"hosts_from_remote.txt"));
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(dest_path).parent_path());  // [SyncSshFileTests.md#no-dir-creation-in-copy]
    CopyResult result = copy_ssh_to_fs(src_path, dest_path);

    EXPECT_EQ(result, CopyResult::OK) << "Kopieren von SSH nach lokal fehlgeschlagen";

    EXPECT_TRUE(fs::exists(dest_path)) << "Lokale Datei wurde nicht erstellt";

    std::ifstream in_file(wstring_to_string(dest_path));
    std::string content((std::istreambuf_iterator<char>(in_file)),
                         std::istreambuf_iterator<char>());
    in_file.close();

    EXPECT_FALSE(content.empty()) << "Kopierte Datei ist leer";
    fs::remove(dest_path);
}

TEST_F(SyncSshFileCopyTest, CopyLargeFileToRemote)
{
    ensure_account_in_config();
    
    std::string large_file_path = tu::concat_path(LOCAL_TEST_DIR, "large_test_file.bin");
    std::ofstream out_file(large_file_path, std::ios::binary);
    std::vector<char> data(1024 * 1024, 'X');  // 1 MB filled with 'X'
    out_file.write(data.data(), data.size());
    out_file.close();
    
    std::wstring src_path = string_to_wstring(large_file_path);  // [SyncSshFileTests.md#src-under-local-test-dir]
    std::wstring dest_path = L"localhost:" + tu::concat_ssh_temp(L"large_test_file_remote.bin");

    CopyResult result = copy_fs_to_ssh(src_path, dest_path);

    EXPECT_EQ(result, CopyResult::OK) << "Kopieren der großen Datei fehlgeschlagen";

    SshFileInspector inspector(create_laptop_account());
    RemoteFileInfo info = inspector.inspect_file(tu::concat_ssh_temp("large_test_file_remote.bin"));

    EXPECT_TRUE(info.exists);
    EXPECT_EQ(info.size_bytes, 1024 * 1024) << "Dateigröße stimmt nicht überein";
    ssh_remove_file(dest_path);
}

TEST_F(SyncSshFileCopyTest, CopyNonExistentSourceFile)
{
    ensure_account_in_config();
    
    std::wstring src_path = tu::concat_temp(L"ssh_copy_test/non_existent_file.txt");
    std::wstring dest_path = L"localhost:" + tu::concat_ssh_temp(L"should_not_be_created.txt");
    
    CopyResult result = copy_fs_to_ssh(src_path, dest_path);  // the copy must fail
    
    EXPECT_NE(result, CopyResult::OK) << "Kopieren einer nicht existierenden Datei sollte fehlschlagen";
}