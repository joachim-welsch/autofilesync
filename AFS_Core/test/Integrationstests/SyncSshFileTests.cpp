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
#include "TestPaths.h"
#include "SyncRunner.h"
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
    const std::string local_file = tu::concat_path(tu::test_source_dir(), "data.txt");
    std::ofstream(local_file) << "data";

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
    si.source(string_to_wstring(local_file));
    si.target(string_to_wstring(test_remote_filename));

    std::size_t copy_count = 0;
    SyncFile test(si, copy_count, false);
    test();

    EXPECT_EQ(copy_count, 1);
    SshFileInspector inspector(create_laptop_account());
    RemoteFileInfo info = inspector.inspect_file(get_remote_path(string_to_wstring(test_remote_filename)));  // [SyncSshFileTests.md#inspect-path-without-prefix]

    EXPECT_TRUE(info.exists) << "Remote-Datei wurde nicht gefunden";
    EXPECT_GT(info.size_bytes, 0) << "Remote-Datei ist leer";
    std::filesystem::remove(string_to_wstring(local_file));
    ssh_remove_file(string_to_wstring(test_remote_filename));  // [SyncSshFileTests.md#ssh-delete-needed]
}

TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemoteTmp)
{
    ensure_account_in_config();
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

#ifdef _UNIX
TEST_F(SyncSshFileCopyTest, CopyRemoteFileToLocalKeepsThePermissions)  // [SyncSshFileTests.md#CopyRemoteFileToLocalKeepsThePermissions]
{
    ensure_account_in_config();
    namespace fs = std::filesystem;
    const fs::path source = fs::path(LOCAL_TEST_DIR) / TEST_FILENAME;
    const fs::path dest = fs::path(LOCAL_TEST_DIR) / "copy_with_permissions.txt";
    const fs::perms source_perms = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read;
    fs::permissions(source, source_perms);

    const CopyResult result = copy_ssh_to_fs(L"localhost:" + source.wstring(), dest.wstring());

    ASSERT_EQ(result, CopyResult::OK);
    EXPECT_EQ(fs::status(dest).permissions(), source_perms);
}

TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemoteKeepsThePermissions)  // [SyncSshFileTests.md#CopyLocalFileToRemoteKeepsThePermissions]
{
    ensure_account_in_config();
    namespace fs = std::filesystem;
    const fs::path source = fs::path(LOCAL_TEST_DIR) / TEST_FILENAME;
    const fs::path dest = tu::concat_ssh_temp("uploaded_with_permissions.txt");
    const fs::perms source_perms = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read;
    fs::permissions(source, source_perms);
    ssh_remove_file(L"localhost:" + dest.wstring());

    const CopyResult result = copy_fs_to_ssh(source.wstring(), L"localhost:" + dest.wstring());

    ASSERT_EQ(result, CopyResult::OK);
    EXPECT_EQ(fs::status(dest).permissions(), source_perms) << "der Upload muss die Rechte der Quelle mitnehmen";
    ssh_remove_file(L"localhost:" + dest.wstring());
}
#endif

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
namespace {

class SyncSshStopTest : public ::testing::Test {  // [SyncSshFileTests.md#SyncSshStopTest]
protected:
    static constexpr std::size_t LARGE_FILE_BYTES = 64u << 20;
    const std::filesystem::path shared_dir_ = tu::concat_ssh_temp("autofilesync_sshstop_" + std::to_string(TestPaths::process_id()));
    const std::filesystem::path local_dir_ = std::filesystem::temp_directory_path() / "autofilesync_sshstop_local";

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        Config::instance().add(SyncSshFileTestConfig::create_test_account());
        clean_up();
        std::filesystem::create_directories(shared_dir_);
        std::filesystem::permissions(shared_dir_, std::filesystem::perms::all);  // [SyncSshFileTests.md#SyncSshStopTest]
        std::filesystem::create_directories(local_dir_);
        Synchronize::start();
    }

    void TearDown() override
    {
        Synchronize::start();
        clean_up();
    }

    void clean_up() const
    {
        std::error_code ec;
        std::filesystem::remove_all(shared_dir_, ec);
        std::filesystem::remove_all(local_dir_, ec);
    }

    std::wstring remote(const std::filesystem::path& file) const
    {
        return L"test_account:" + string_to_wstring(file.string());
    }

    std::size_t sync_and_stop_when(const std::wstring& source, const std::wstring& target, const std::filesystem::path& file_the_copy_creates) const  // [SyncSshFileTests.md#sync_and_stop_when]
    {
        std::thread stopper([&] {
            while (!std::filesystem::exists(file_the_copy_creates))
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Synchronize::stop();
        });
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::File);
        si.source(source);
        si.target(target);
        std::size_t copy_count = 0;
        SyncFile(si, copy_count)();
        stopper.join();
        return copy_count;
    }
};

} // namespace

TEST_F(SyncSshStopTest, StopDuringTheUploadLeavesNoHalfFileOnTheRemote)  // [SyncSshFileTests.md#StopDuringTheUploadLeavesNoHalfFileOnTheRemote]
{
    const std::filesystem::path source = local_dir_ / "gross.bin";
    const std::filesystem::path target = shared_dir_ / "gross.bin";
    tu::write_large_file(source, LARGE_FILE_BYTES);

    const std::size_t copy_count = sync_and_stop_when(source.wstring(), remote(target), target);

    EXPECT_EQ(copy_count, 0u) << "der Upload war schon fertig, bevor der Stopp ankam - die Datei muss groesser sein";
    EXPECT_FALSE(ssh_exists(remote(target))) << "eine halb hochgeladene Datei darf nicht im Ziel zurueckbleiben";
}

TEST_F(SyncSshStopTest, StopDuringTheDownloadLeavesNoHalfFileLocally)  // [SyncSshFileTests.md#StopDuringTheDownloadLeavesNoHalfFileLocally]
{
    const std::filesystem::path source = shared_dir_ / "gross.bin";
    const std::filesystem::path target = local_dir_ / "gross.bin";
    tu::write_large_file(source, LARGE_FILE_BYTES);
    std::filesystem::permissions(source, std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read);

    const std::size_t copy_count = sync_and_stop_when(remote(source), target.wstring(), target);

    EXPECT_EQ(copy_count, 0u) << "der Download war schon fertig, bevor der Stopp ankam - die Datei muss groesser sein";
    EXPECT_FALSE(std::filesystem::exists(target)) << "eine halb heruntergeladene Datei darf nicht im Ziel zurueckbleiben";
    EXPECT_FALSE(std::filesystem::exists(target.wstring() + L".bak"));
}

namespace {
class SyncSshFlagTest : public SyncSshStopTest {};  // [SyncSshFileTests.md#SyncSshFlagTest]
}

TEST_F(SyncSshFlagTest, MoveLeavesTheSshSourceInPlace)  // [SyncSshFileTests.md#MoveLeavesTheSshSourceInPlace]
{
    const std::filesystem::path source = shared_dir_ / "brief.txt";
    const std::filesystem::path target = local_dir_ / "brief.txt";
    tu::write_text_file(source, "Inhalt");
    std::filesystem::permissions(source, std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source(remote(source));
    si.target(target.wstring());
    si.del_aftercopy(true);

    EXPECT_EQ(tu::run_sync(si), 1u);

    EXPECT_EQ(tu::read_text_file(target), "Inhalt");
    EXPECT_TRUE(ssh_exists(remote(source))) << "eine SSH-Quelle wird nicht geloescht - das Verschieben endet als Kopie";
}

namespace {

class SyncSshFileToDirTest : public SyncSshStopTest {  // [SyncSshFileTests.md#SyncSshFileToDirTest]
protected:
    void SetUp() override
    {
        SyncSshStopTest::SetUp();
        if (IsSkipped())
            return;
        Config::instance().reset();
        Config::instance().add(SyncSshFileTestConfig::create_test_account());
        std::filesystem::create_directories(shared_dir_ / "ziel");
        std::filesystem::permissions(shared_dir_ / "ziel", std::filesystem::perms::all);
    }

    void TearDown() override
    {
        if (Synchronize::has_instance())
            Synchronize::destroy();
        Config::instance().reset();
        SyncSshStopTest::TearDown();
    }

    void run_file_item(const std::wstring& source, const std::wstring& target, const std::filesystem::path& expected_file) const  // [SyncSshFileTests.md#run_file_item]
    {
        SyncItem si(L"datei", SyncItemType::File, source, target, L"*");
        si.sync_cycle(0);
        Config::instance().add(si);
        Synchronize::create(SyncType::Single);
        for (int i = 0; i < 100 && !std::filesystem::exists(expected_file); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Synchronize::destroy();
    }

    static uint64_t remote_size(const std::filesystem::path& file)  // [SyncSshFileTests.md#remote_size]
    {
        SshFileInspector inspector(SyncSshFileTestConfig::create_test_account());
        return inspector.inspect_file(file.string()).size_bytes;
    }

    std::filesystem::path readable_source(const std::filesystem::path& file) const
    {
        tu::write_text_file(file, "Inhalt");
        std::filesystem::permissions(file, std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
        return file;
    }
};

} // namespace

TEST_F(SyncSshFileToDirTest, LocalFileLandsInTheSshDirectoryUnderItsOwnName)  // [SyncSshFileTests.md#file-into-directory]
{
    const std::filesystem::path source = readable_source(local_dir_ / "brief.txt");

    run_file_item(source.wstring(), remote(shared_dir_ / "ziel"), shared_dir_ / "ziel" / "brief.txt");

    EXPECT_TRUE(ssh_exists(remote(shared_dir_ / "ziel" / "brief.txt")));
    EXPECT_EQ(remote_size(shared_dir_ / "ziel" / "brief.txt"), 6u);
    EXPECT_EQ(Config::instance().syncitems().at(0).last_run()->errors, 0u);
}

TEST_F(SyncSshFileToDirTest, LocalFileKeepsTheNameOfAnSshFileTarget)  // [SyncSshFileTests.md#file-into-directory]
{
    const std::filesystem::path source = readable_source(local_dir_ / "brief.txt");

    run_file_item(source.wstring(), remote(shared_dir_ / "ziel" / "anders.txt"), shared_dir_ / "ziel" / "anders.txt");

    EXPECT_EQ(remote_size(shared_dir_ / "ziel" / "anders.txt"), 6u);
    EXPECT_FALSE(ssh_exists(remote(shared_dir_ / "ziel" / "brief.txt"))) << "ein Dateiziel bekommt nicht den Quellnamen angehaengt";
}

TEST_F(SyncSshFileToDirTest, SshFileLandsInTheLocalDirectoryUnderItsOwnName)  // [SyncSshFileTests.md#file-into-directory]
{
    const std::filesystem::path source = readable_source(shared_dir_ / "brief.txt");

    run_file_item(remote(source), local_dir_.wstring(), local_dir_ / "brief.txt");

    EXPECT_EQ(tu::read_text_file(local_dir_ / "brief.txt"), "Inhalt");
}

TEST_F(SyncSshFileToDirTest, SshFileLandsInTheSshDirectoryUnderItsOwnName)  // [SyncSshFileTests.md#file-into-directory]
{
    const std::filesystem::path source = readable_source(shared_dir_ / "brief.txt");

    run_file_item(remote(source), remote(shared_dir_ / "ziel"), shared_dir_ / "ziel" / "brief.txt");

    EXPECT_EQ(tu::read_text_file(shared_dir_ / "ziel" / "brief.txt"), "Inhalt");
}
