/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncSshTreeTests.h"
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

namespace SyncSshTreeTestConfig {
    const std::string SRC_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_src");
    const std::string DEST_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_dest");
    
    inline SshAccount create_test_account(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.set_name("test_tree_account");
        acc.set_user(user);
        acc.set_host(host);
        acc.set_password(password);
        return acc;
    }
}

class SyncSshTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        namespace fs = std::filesystem;

        SKIP_WITHOUT_SSH_SERVER();

        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        fs::create_directories(SyncSshTreeTestConfig::SRC_TEST_DIR);
        
        SshAccount acc;
        acc.set_name("localhost");
        acc.set_user(SSH_USER);
        acc.set_host(SSH_HOST);
        acc.set_password(SSH_PASSWORD);
            Config::instance().add(acc);

            static bool ssh_dest_cleaned = false;  // [SyncSshTreeTests.md#remote-cleanup-once-per-process]
            if (!ssh_dest_cleaned) {
                tu::erase_ssh_test_dir_variants(
                    L"localhost:" + std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()),
                    { L"_reverse", L"_empty", L"_mixed", L"_many", L"_deep", L"_types", L"_large", L"_project", L"_special" });
                ssh_dest_cleaned = true;
            }

            Synchronize::start();
        }

    void TearDown() override {
        namespace fs = std::filesystem;
        
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
};

TEST_F(SyncSshTreeTest, SyncLocalTreeToRemoteWithNestedDirectories)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    fs::create_directories(tu::concat_path(tu::concat_path(local_dir, "subdir1"), "subdir2"));
    fs::create_directories(tu::concat_path(local_dir, "subdir3"));
    
    std::ofstream(tu::concat_path(local_dir, "root_file.txt")) << "root content";
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "subdir1"), "sub1_file.txt")) << "subdir1 content";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "subdir1"), "subdir2"), "sub2_file.txt")) << "subdir2 content";
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "subdir3"), "sub3_file.txt")) << "subdir3 content";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    
    RemoteFileInfo root_file = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR, "root_file.txt"));
    RemoteFileInfo sub1_file = inspector.inspect_file(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir1"), "sub1_file.txt"));
    RemoteFileInfo sub2_file = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir1"), "subdir2"), "sub2_file.txt"));
    RemoteFileInfo sub3_file = inspector.inspect_file(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir3"), "sub3_file.txt"));
    
    EXPECT_TRUE(root_file.exists) << "root_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub1_file.exists) << "sub1_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub2_file.exists) << "sub2_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub3_file.exists) << "sub3_file.txt wurde nicht auf Remote gefunden";

    {  // [SyncSshTreeTests.md#ssh-cleanup-needed]
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()));
    }
}

TEST_F(SyncSshTreeTest, SyncRemoteTreeToLocal)
{
    namespace fs = std::filesystem;
    
    std::string local_setup_dir = tu::concat_path(SyncSshTreeTestConfig::SRC_TEST_DIR, "setup_for_remote");
    fs::create_directories(tu::concat_path(local_setup_dir, "subdir"));
    std::ofstream(tu::concat_path(local_setup_dir, "remote_file.txt")) << "remote content";
    std::ofstream(tu::concat_path(tu::concat_path(local_setup_dir, "subdir"), "nested_file.txt")) << "nested content";
    
    std::wstring setup_src_path = std::wstring(local_setup_dir.begin(), local_setup_dir.end()) + PATH_SEPARATOR_W;
    std::wstring remote_dest_path = L"localhost:" + 
                                  std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                              SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse";
    
    SyncItem setup_si;
    setup_si.sync_cycle(0);
    setup_si.sync_item_type(SyncItemType::Tree);
    setup_si.source(setup_src_path);
    setup_si.target(remote_dest_path);
    
    std::size_t setup_copy_count = 0;
    SyncTree setup_sync(setup_si, setup_copy_count, false);
    setup_sync();
    
    Synchronize::start();
    std::wstring src_path = L"localhost:" + 
                           std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(),
                                       SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse";
    std::wstring dest_path = std::wstring(SyncSshTreeTestConfig::SRC_TEST_DIR.begin(),
                                         SyncSshTreeTestConfig::SRC_TEST_DIR.end()) + PATH_SEPARATOR_W + L"remote_copy";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    EXPECT_TRUE(fs::exists(tu::concat_path(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy")));
    
    EXPECT_TRUE(fs::exists(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy"), "remote_file.txt")))
        << "remote_file.txt wurde nicht von Remote kopiert";
    EXPECT_TRUE(fs::exists(tu::concat_path(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy"), "subdir"), "nested_file.txt")))
        << "nested_file.txt wurde nicht von Remote kopiert";

    {  // [SyncSshTreeTests.md#reverse-cleanup-path]
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse");
    }
}

TEST_F(SyncSshTreeTest, SyncEmptyTreeToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    fs::create_directories(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "empty1"), "empty2"), "empty3"), ""));
    fs::create_directories(tu::concat_path(local_dir, "empty4"));
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_empty";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    
    EXPECT_NO_THROW(test());  // should throw nothing

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncMixedTreeToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    fs::create_directories(tu::concat_path(local_dir, "dir1"));
    fs::create_directories(tu::concat_path(tu::concat_path(local_dir, "dir2"), "nested"));
    
    std::ofstream(tu::concat_path(local_dir, "file1.txt")) << "content1";
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "dir1"), "file2.txt")) << "content2";
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "dir1"), "file3.md")) << "# Markdown";
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "dir2"), "file4.txt")) << "content4";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "dir2"), "nested"), "file5.txt")) << "content5";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_mixed";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    
    RemoteFileInfo file1 = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "file1.txt"));
    RemoteFileInfo file2 = inspector.inspect_file(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir1"), "file2.txt"));
    RemoteFileInfo file3 = inspector.inspect_file(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir1"), "file3.md"));
    RemoteFileInfo file4 = inspector.inspect_file(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir2"), "file4.txt"));
    RemoteFileInfo file5 = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir2"), "nested"), "file5.txt"));
    
    EXPECT_TRUE(file1.exists) << "file1.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file2.exists) << "file2.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file3.exists) << "file3.md wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file4.exists) << "file4.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file5.exists) << "file5.txt wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncTreeWithManyFilesToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    fs::create_directories(tu::concat_path(local_dir, "many_files"));
    
    const int file_count = 50;
    for (int i = 0; i < file_count; ++i) {
        std::string filename = tu::concat_path(tu::concat_path(local_dir, "many_files"), "file_" + std::to_string(i) + ".txt");
        std::ofstream(filename) << "Content of file " << i;
    }
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_many";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    RemoteFileInfo info0 = inspector.inspect_file(
        tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_0.txt"));
    RemoteFileInfo info25 = inspector.inspect_file(
        tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_25.txt"));
    RemoteFileInfo info49 = inspector.inspect_file(
        tu::concat_path(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_49.txt"));
    
    EXPECT_TRUE(info0.exists) << "file_0.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info25.exists) << "file_25.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info49.exists) << "file_49.txt wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncDeepHierarchyToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    std::string deep_path = local_dir;
    for (int i = 0; i < 8; ++i) {
        deep_path = tu::concat_path(deep_path, "level" + std::to_string(i));
    }
    fs::create_directories(deep_path);
    std::ofstream(tu::concat_path(deep_path, "deep_file.txt")) << "Deep content";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_deep";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    std::string remote_deep_path = SyncSshTreeTestConfig::DEST_TEST_DIR + "_deep";
    for (int i = 0; i < 8; ++i) {
        remote_deep_path = tu::concat_path(remote_deep_path, "level" + std::to_string(i));
    }
    RemoteFileInfo deep_file = inspector.inspect_file(tu::concat_path(remote_deep_path, "deep_file.txt"));
    
    EXPECT_TRUE(deep_file.exists) << "deep_file.txt wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncDifferentFileTypesToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    std::ofstream(tu::concat_path(local_dir, "document.txt")) << "Text file";
    std::ofstream(tu::concat_path(local_dir, "script.sh")) << "#!/bin/bash\necho 'test'";
    std::ofstream(tu::concat_path(local_dir, "config.json")) << "{\"key\": \"value\"}";
    std::ofstream(tu::concat_path(local_dir, "data.xml")) << "<?xml version=\"1.0\"?><root/>";
    std::ofstream(tu::concat_path(local_dir, "README.md")) << "# README";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_types";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    
    RemoteFileInfo txt = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "document.txt"));
    RemoteFileInfo sh = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "script.sh"));
    RemoteFileInfo json = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "config.json"));
    RemoteFileInfo xml = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "data.xml"));
    RemoteFileInfo md = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "README.md"));
    
    EXPECT_TRUE(txt.exists) << "document.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sh.exists) << "script.sh wurde nicht auf Remote gefunden";
    EXPECT_TRUE(json.exists) << "config.json wurde nicht auf Remote gefunden";
    EXPECT_TRUE(xml.exists) << "data.xml wurde nicht auf Remote gefunden";
    EXPECT_TRUE(md.exists) << "README.md wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncLargeFileToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    std::ofstream large_file(tu::concat_path(local_dir, "large_file.dat"), std::ios::binary);
    std::string chunk(1024, 'X'); // 1 KB
    for (int i = 0; i < 1024; ++i) { // 1 MB
        large_file << chunk;
    }
    large_file.close();
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_large";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    RemoteFileInfo large_file_info = inspector.inspect_file(
        SyncSshTreeTestConfig::DEST_TEST_DIR + "_large/large_file.dat");
    
    EXPECT_TRUE(large_file_info.exists) << "large_file.dat wurde nicht auf Remote gefunden";
    
    auto source_size = fs::file_size(tu::concat_path(local_dir, "large_file.dat"));
    EXPECT_EQ(large_file_info.size_bytes, source_size);

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncComplexProjectStructureToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    fs::create_directories(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "main"));
    fs::create_directories(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "utils"));
    fs::create_directories(tu::concat_path(tu::concat_path(local_dir, "project"), "tests"));
    fs::create_directories(tu::concat_path(tu::concat_path(local_dir, "project"), "docs"));
    fs::create_directories(tu::concat_path(tu::concat_path(local_dir, "project"), "build"));
    
    std::ofstream(tu::concat_path(tu::concat_path(local_dir, "project"), "README.md")) << "# Project";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "main"), "main.cpp")) << "int main() {}";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "main"), "config.h")) << "#pragma once";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "utils"), "helper.cpp")) << "// helper";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "src"), "utils"), "helper.h")) << "// helper.h";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "tests"), "test_main.cpp")) << "// test";
    std::ofstream(tu::concat_path(tu::concat_path(tu::concat_path(local_dir, "project"), "docs"), "manual.txt")) << "Manual";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_project";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    
    std::string remote_base = tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_project", "project");
    
    RemoteFileInfo readme = inspector.inspect_file(tu::concat_path(remote_base, "README.md"));
    RemoteFileInfo main_cpp = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(tu::concat_path(remote_base, "src"), "main"), "main.cpp"), ""));
    RemoteFileInfo config_h = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(remote_base, "src"), "main"), "config.h"));
    RemoteFileInfo helper_cpp = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(remote_base, "src"), "utils"), "helper.cpp"));
    RemoteFileInfo helper_h = inspector.inspect_file(tu::concat_path(tu::concat_path(tu::concat_path(remote_base, "src"), "utils"), "helper.h"));
    RemoteFileInfo test_main = inspector.inspect_file(tu::concat_path(tu::concat_path(remote_base, "tests"), "test_main.cpp"));
    RemoteFileInfo manual = inspector.inspect_file(tu::concat_path(tu::concat_path(remote_base, "docs"), "manual.txt"));
    
    EXPECT_TRUE(readme.exists) << "README.md wurde nicht auf Remote gefunden";
    EXPECT_TRUE(main_cpp.exists) << "main.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(config_h.exists) << "config.h wurde nicht auf Remote gefunden";
    EXPECT_TRUE(helper_cpp.exists) << "helper.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(helper_h.exists) << "helper.h wurde nicht auf Remote gefunden";
    EXPECT_TRUE(test_main.exists) << "test_main.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(manual.exists) << "manual.txt wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

TEST_F(SyncSshTreeTest, SyncFilesWithSpecialCharactersToRemote)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    std::ofstream(tu::concat_path(local_dir, "file_with_spaces.txt")) << "spaces";
    std::ofstream(tu::concat_path(local_dir, "file-with-dashes.txt")) << "dashes";
    std::ofstream(tu::concat_path(local_dir, "file_with_underscores.txt")) << "underscores";
    std::ofstream(tu::concat_path(local_dir, "file.multiple.dots.txt")) << "dots";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_special";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();
    
    SshFileInspector inspector(SyncSshTreeTestConfig::create_test_account());
    
    RemoteFileInfo spaces = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file_with_spaces.txt"));
    RemoteFileInfo dashes = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file-with-dashes.txt"));
    RemoteFileInfo underscores = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file_with_underscores.txt"));
    RemoteFileInfo dots = inspector.inspect_file(tu::concat_path(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file.multiple.dots.txt"));
    
    EXPECT_TRUE(spaces.exists) << "file_with_spaces.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(dashes.exists) << "file-with-dashes.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(underscores.exists) << "file_with_underscores.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(dots.exists) << "file.multiple.dots.txt wurde nicht auf Remote gefunden";

    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}
