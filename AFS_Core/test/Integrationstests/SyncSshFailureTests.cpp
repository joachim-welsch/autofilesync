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
#ifdef _UNIX
#include "Config.h"
#include "CopyLog.h"
#include "FileUtil.h"
#include "SshHostKeyVerification.h"
#include "SshTest.h"
#include "SyncItem.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Util.h"
#include <chrono>
#include <filesystem>

using namespace AFS_Core;
using namespace SSHTest;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

const std::string PLAIN_ACCOUNT = "afs_failure_plain";
const std::string PINNED_ACCOUNT = "afs_failure_pinned";  // [SyncSshFailureTests.md#pinned-account]

struct Outcome {
    std::size_t copies;
    SyncReport report;
};

class SyncSshFailureTest : public ::testing::Test {  // [SyncSshFailureTests.md#SyncSshFailureTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("ssh_failure_" + std::to_string(TestPaths::process_id()));
    const fs::path local_dir_ = root_ / "lokal";
    const fs::path server_dir_ = root_ / "server";

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        fs::remove_all(root_);
        fs::create_directories(local_dir_);
        fs::create_directories(server_dir_);
        fs::permissions(root_, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);
        Config::instance().add(account(PLAIN_ACCOUNT, ""));
        std::string error;
        const auto host_key = fetch_ssh_host_key(account("", ""), error);
        ASSERT_TRUE(host_key.has_value()) << error;
        Config::instance().add(account(PINNED_ACCOUNT, *host_key));
        Synchronize::start();
    }

    void TearDown() override
    {
        for (const std::string& name : {PLAIN_ACCOUNT, PINNED_ACCOUNT})
            remove_account(name);
        std::error_code ec;
        fs::permissions(server_dir_, fs::perms::owner_all, ec);
        fs::remove_all(root_, ec);
    }

    static SshAccount account(const std::string& name, const std::string& host_key)
    {
        SshAccount account;
        account.set_name(name);
        account.set_user(SSH_USER);
        account.set_host(SSH_HOST);
        account.set_password(SSH_PASSWORD);
        account.set_host_key(host_key);
        return account;
    }

    static void remove_account(const std::string& name)
    {
        const auto accounts = Config::instance().ssh_accounts_snapshot();
        for (std::size_t i = accounts.size(); i-- > 0;)
            if (accounts[i].name() == name)
                Config::instance().remove_ssh_account(i);
    }

    std::wstring on_server(const std::string& account_name) const
    {
        return util::string_to_wstring(account_name + ":" + server_dir_.string());
    }

    static Outcome run(const SyncItem& si)  // [LocalPermissionTests.md#run]
    {
        Outcome outcome;
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si);
        return outcome;
    }
};

} // namespace

TEST_F(SyncSshFailureTest, UnlistableRemoteSourceDirIsReportedAndDeletesNothingItStillHas)  // [SyncSshFailureTests.md#UnlistableRemoteSourceDirIsReportedAndDeletesNothingItStillHas]
{
    tu::write_text_file(server_dir_ / "a.txt", "auf beiden Seiten");
    tu::write_text_file(server_dir_ / "neu.txt", "nur auf dem Server");
    tu::write_text_file(local_dir_ / "a.txt", "auf beiden Seiten");
    fs::permissions(server_dir_, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);  // [SyncSshFailureTests.md#listing-refused]
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(on_server(PLAIN_ACCOUNT));
    si.target(local_dir_.wstring());
    si.delete_in_target(true);

    const Outcome outcome = run(si);

    EXPECT_TRUE(fs::exists(local_dir_ / "a.txt")) << "eine nicht auflistbare Quelle ist keine leere Quelle";
    EXPECT_FALSE(fs::exists(local_dir_ / "neu.txt"));
    EXPECT_FALSE(outcome.report.successful()) << "ein Lauf, der die Quelle nicht auflisten konnte, darf nicht als sauber gelten";
}

TEST_F(SyncSshFailureTest, PartFileThatCannotReplaceTheTargetLeavesTheOldFile)  // [SyncSshFailureTests.md#PartFileThatCannotReplaceTheTargetLeavesTheOldFile]
{
    tu::write_text_file(local_dir_ / "a.txt", "neu");
    tu::write_text_file(server_dir_ / "a.txt", "alt", seconds(3600));
    fs::permissions(server_dir_, fs::perms::all | fs::perms::sticky_bit);  // [SyncSshFailureTests.md#sticky-directory]
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PINNED_ACCOUNT) + L"/a.txt");

    const Outcome outcome = run(si);

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(server_dir_ / "a.txt"), "alt") << "die alte Datei muss bleiben, wenn die neue sie nicht ersetzen kann";
    EXPECT_FALSE(fs::exists(server_dir_ / ("a.txt" + util::wstring_to_string(PART_SUFFIX)))) << "die Teildatei darf nicht liegen bleiben";
}
#endif
