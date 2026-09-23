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
#include "SyncItem.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "CopyLog.h"
#include "Config.h"
#include "SshAccount.h"
#include "TestPaths.h"
#include <StrUtil.h>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace AFS_Core;

using util::wstring_to_string;
namespace fs = std::filesystem;

namespace {  // [SshPathEncodingTests.md#overview]
    constexpr char ACCOUNT_NAME[] = "mmserver";
    constexpr char UNREACHABLE_HOST[] = "autofilesync.invalid";  // [SshPathEncodingTests.md#UNREACHABLE_HOST]
    const std::wstring REMOTE_DIR = L"mmserver:/tmp/autofilesync_test/ssh_target";
    const std::wstring LOCAL_SOURCE_DIR = TestPaths::scratch_dir() + L"/ssh_path_encoding_src";
}

class SshPathEncodingTest : public ::testing::Test {
    std::vector<std::string> added_accounts_;
protected:
    void add_account(const std::string& name)
    {
        SshAccount account;
        account.set_name(name);
        account.set_user("testuser");
        account.set_host(UNREACHABLE_HOST);
        account.set_password("testpassword");
        Config::instance().add(account);
        added_accounts_.push_back(name);
    }
    void SetUp() override { add_account(ACCOUNT_NAME); }
    void TearDown() override  // [SshPathEncodingTests.md#TearDown]
    {
        for (const auto& name : added_accounts_) {
            const auto accounts = Config::instance().ssh_accounts_snapshot();
            for (std::size_t i = 0; i < accounts.size(); ++i)
                if (accounts[i].name() == name) {
                    Config::instance().remove_ssh_account(i);
                    break;
                }
        }
        added_accounts_.clear();
    }
};

TEST_F(SshPathEncodingTest, SshTargetWithUmlautStaysConvertible)
{
    const std::wstring target = REMOTE_DIR + L"/Bücher.txt";
    SyncItem item;
    item.target(target);
    ASSERT_TRUE(item.ssh_target());
    EXPECT_NO_THROW(wstring_to_string(item.target()));
    EXPECT_EQ(item.target(), target);
}

TEST_F(SshPathEncodingTest, SshSourceWithUmlautStaysConvertible)
{
    const std::wstring source = REMOTE_DIR + L"/Bücher.txt";
    SyncItem item;
    item.source(source);
    ASSERT_TRUE(item.ssh_source());
    EXPECT_NO_THROW(wstring_to_string(item.source()));
    EXPECT_EQ(item.source(), source);
}

TEST_F(SshPathEncodingTest, SshAccountNameWithUmlautStaysConvertible)  // [SshPathEncodingTests.md#SshAccountNameWithUmlautStaysConvertible]
{
    add_account("Büro");
    const std::wstring target = L"Büro:/tmp/autofilesync_test/ssh_target";
    SyncItem item;
    item.target(target);
    ASSERT_TRUE(item.ssh_target());
    EXPECT_NO_THROW(wstring_to_string(item.target()));
    EXPECT_EQ(item.target(), target);
}

TEST_F(SshPathEncodingTest, TreeSyncToSshTargetDoesNotAbortOnUmlautFilename)  // [SshPathEncodingTests.md#TreeSyncToSshTargetDoesNotAbortOnUmlautFilename]
{
    fs::remove_all(LOCAL_SOURCE_DIR);
    fs::create_directories(LOCAL_SOURCE_DIR);
    std::ofstream(fs::path(LOCAL_SOURCE_DIR) / "Bücher.txt") << "Inhalt";
    std::ofstream(fs::path(LOCAL_SOURCE_DIR) / "zzz_after.txt") << "Inhalt";

    Synchronize::start();
    SyncItem item;
    item.sync_cycle(0);
    item.sync_item_type(SyncItemType::Tree);
    item.source(LOCAL_SOURCE_DIR);
    item.target(REMOTE_DIR);

    SyncReport report;
    SyncReportGuard guard(&report);
    std::size_t copy_count = 0;
    SyncTree tree(item, copy_count, false);
    tree();

    for (const auto& error : report.errors())
        EXPECT_EQ(error.find(L"wstring_convert"), std::wstring::npos)
            << "Konvertierungsfehler bricht die Synchronisation ab: " << error;

    fs::remove_all(LOCAL_SOURCE_DIR);
}
