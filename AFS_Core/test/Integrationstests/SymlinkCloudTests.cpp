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
#include "ScopedLog.h"
#include "SyncFileSsh.h"
#include "SyncPlan.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include "Logging.h"
#include "StrUtil.h"
#include "WebDavTestServer.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

const std::string ACCOUNT_NAME = "linkcloud";

struct Outcome {
    std::size_t copies = 0;
    SyncReport report;
};

class SymlinkCloudTest : public ::testing::Test {  // [SymlinkCloudTests.md#SymlinkCloudTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_linkcloud_" + std::to_string(TestPaths::process_id()));
    const fs::path remote_ = root_ / "server";
    const fs::path local_ = root_ / "lokal";
    const tu::ScopedLog log_{fs::temp_directory_path() / ("autofilesync_linkcloud_" + std::to_string(TestPaths::process_id()) + ".log"), LogLevel::Info};
    std::unique_ptr<tu::WebDavTestServer> server_;

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(remote_);
        fs::create_directories(local_ / "echt");
        server_ = std::make_unique<tu::WebDavTestServer>(remote_, true);
        if (!server_->running())
            GTEST_SKIP() << "The WebDAV test server could not be started (needs python3).";
        SshAccount account;
        account.set_name(ACCOUNT_NAME);
        account.set_url(server_->url());
        account.set_user(tu::WebDavTestServer::USER);
        account.set_password(tu::WebDavTestServer::PASSWORD);
        Config::instance().add(account);
        tu::write_text_file(local_ / "echt" / "a.txt", "a");
        tu::write_text_file(local_ / "b.txt", "b");
        fs::create_symlink("b.txt", local_ / "zeiger.txt");
        fs::create_directory_symlink("echt", local_ / "ordner");
        fs::create_symlink("weg.txt", local_ / "tot.txt");
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        server_.reset();
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    static std::wstring cloud(const std::string& path = "") { return util::string_to_wstring(ACCOUNT_NAME + ":/" + path); }

    SyncItem upload_item(SymlinkMode mode = SymlinkMode::AsLink) const
    {
        SyncItem si(L"Hoch", SyncItemType::Tree, local_.wstring(), cloud(), L"*");
        si.sync_cycle(0);
        si.symlink_mode(mode);
        return si;
    }

    static void run(const SyncItem& si, Outcome& outcome, SyncState* state = nullptr)  // [DiskFullTests.md#run]
    {
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si, state);
    }

    int logged(const char* message_id) const  // [SymlinkTests.md#skipped_links]
    {
        std::ifstream in(log_.path());
        const std::string message = translation::translate_utf8(message_id);
        int count = 0;
        for (std::string line; std::getline(in, line);)
            if (line.find(message) != std::string::npos)
                ++count;
        return count;
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }
};

} // namespace

TEST_F(SymlinkCloudTest, CloudAccountKeepsNoLinks)  // [SymlinkCloudTests.md#CloudAccountKeepsNoLinks]
{
    EXPECT_FALSE(ssh_keeps_links(cloud("zeiger.txt")));
    EXPECT_EQ(ssh_create_link(L"b.txt", cloud("zeiger.txt")), fsu::FileInfoResult::LinksNotSupported);
    EXPECT_FALSE(ssh_link_info(cloud("zeiger.txt")).has_value());
    EXPECT_TRUE(names_in(remote_).empty()) << "asking must not create anything";
}

TEST_F(SymlinkCloudTest, LinksAreSkippedForACloudTargetWithOneWarningAndNoError)  // [SymlinkCloudTests.md#LinksAreSkippedForACloudTargetWithOneWarningAndNoError]
{
    Outcome first, second;

    run(upload_item(), first);
    run(upload_item(), second);

    EXPECT_TRUE(first.report.successful()) << "a cloud without links is no failure of the run";
    EXPECT_EQ(first.copies, 2u) << "the two files";
    EXPECT_EQ(second.copies, 0u);
    EXPECT_EQ(names_in(remote_), (std::set<std::string>{"b.txt", "echt"})) << "no link, and no content in a link's place";
    EXPECT_EQ(names_in(remote_ / "echt"), std::set<std::string>{"a.txt"});
    EXPECT_EQ(logged("links_not_supported_skipped"), 1) << "one line for three links and two passes";
}

TEST_F(SymlinkCloudTest, TwoWaySyncWithACloudNeverDeletesTheLocalLinkAndRemembersNone)  // [SymlinkCloudTests.md#TwoWaySyncWithACloudNeverDeletesTheLocalLinkAndRemembersNone]
{
    SyncItem si = upload_item();
    si.two_way_sync(true);
    SyncState state(si.source(), si.target());
    Outcome first, second;

    run(si, first, &state);
    run(si, second, &state);

    EXPECT_TRUE(second.report.successful());
    EXPECT_EQ(fs::read_symlink(local_ / "zeiger.txt"), fs::path("b.txt"));
    EXPECT_TRUE(fs::is_symlink(local_ / "ordner"));
    EXPECT_TRUE(fs::is_symlink(local_ / "tot.txt")) << "a link the cloud cannot hold was not deleted there, so it is not deleted here";
    EXPECT_EQ(state.find(L"zeiger.txt"), nullptr) << "what was never brought in line must not be remembered as if it had been";
    EXPECT_NE(state.find(L"b.txt"), nullptr);
    EXPECT_EQ(names_in(remote_), (std::set<std::string>{"b.txt", "echt"}));
}

TEST_F(SymlinkCloudTest, FileInTheCloudUnderTheNameOfALocalLinkIsLeftAlone)  // [SymlinkCloudTests.md#FileInTheCloudUnderTheNameOfALocalLinkIsLeftAlone]
{
    tu::write_text_file(remote_ / "zeiger.txt", "liegt schon in der Cloud");
    SyncItem si = upload_item();
    si.delete_in_target(true);
    Outcome outcome;

    run(si, outcome);

    EXPECT_EQ(tu::read_text_file(remote_ / "zeiger.txt"), "liegt schon in der Cloud") << "neither deleted nor replaced";
    EXPECT_TRUE(fs::is_symlink(local_ / "zeiger.txt"));
}

TEST_F(SymlinkCloudTest, PreviewPlansNoStepForALinkTheCloudCannotTake)  // [SymlinkCloudTests.md#PreviewPlansNoStepForALinkTheCloudCannotTake]
{
    SyncPlan plan;
    std::size_t count = 0;
    {
        SyncPlanGuard guard(&plan);
        SyncTree preview(upload_item(), count, true);
        preview();
    }

    EXPECT_EQ(plan.count(), 2u) << "the two files";
    EXPECT_EQ(count, 2u);
    for (const PlannedStep& step : plan.steps())
        EXPECT_FALSE(step.link);
}

TEST_F(SymlinkCloudTest, DefaultModeStillUploadsAFileLinkAsItsContent)  // [SymlinkCloudTests.md#DefaultModeStillUploadsAFileLinkAsItsContent]
{
    Outcome outcome;

    run(upload_item(SymlinkMode::FileContent), outcome);

    EXPECT_EQ(tu::read_text_file(remote_ / "zeiger.txt"), "b");
    EXPECT_FALSE(fs::exists(remote_ / "ordner")) << "a directory link is skipped as before";
}
#endif
