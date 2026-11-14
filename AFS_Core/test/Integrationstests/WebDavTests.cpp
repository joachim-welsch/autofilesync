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
#include "WebDav.h"
#include "Config.h"
#include "ScopedDeleteMode.h"
#include "SshUnreachable.h"
#include "SyncFileSsh.h"
#include "SyncItem.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Util.h"
#include "WebDavTestServer.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

const std::string ACCOUNT_NAME = "davtest";

class WebDavTest : public ::testing::Test {  // [WebDavTests.md#WebDavTest]
protected:
    fs::path root_;
    fs::path remote_;
    fs::path local_;
    std::unique_ptr<tu::WebDavTestServer> server_;
    SshAccount account_;

    virtual bool server_keeps_time() const { return true; }

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        root_ = fs::temp_directory_path() / ("autofilesync_webdavtest_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(root_);
        remote_ = root_ / "server";
        local_ = root_ / "lokal";
        fs::create_directories(remote_);
        fs::create_directories(local_);
        server_ = std::make_unique<tu::WebDavTestServer>(remote_, server_keeps_time());
        if (!server_->running())
            GTEST_SKIP() << "The WebDAV test server could not be started (needs python3).";
        account_.set_name(ACCOUNT_NAME);
        account_.set_url(server_->url());
        account_.set_user(tu::WebDavTestServer::USER);
        account_.set_password(tu::WebDavTestServer::PASSWORD);
        Config::instance().add(account_);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        server_.reset();
        fs::remove_all(root_);
    }

    static void write(const fs::path& file, const std::string& content)
    {
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << content;
    }

    static std::string content_of(const fs::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    static void set_age(const fs::path& file, std::chrono::hours age)
    {
        fs::last_write_time(file, fs::file_time_type::clock::now() - age);
    }

    std::wstring remote_path(const std::string& path = "") const
    {
        return util::string_to_wstring(ACCOUNT_NAME + ":/" + path);
    }

    SyncItem upload_item(SyncItemType type = SyncItemType::Tree) const
    {
        return SyncItem(L"Hoch", type, local_.wstring(), remote_path(), L"*");
    }

    SyncItem download_item(SyncItemType type = SyncItemType::Tree) const
    {
        return SyncItem(L"Herunter", type, remote_path(), local_.wstring(), L"*");
    }
};

class PlainWebDavTest : public WebDavTest {  // [WebDavTests.md#PlainWebDavTest]
protected:
    bool server_keeps_time() const override { return false; }
};

TEST_F(WebDavTest, StatTellsFileDirectoryAndAbsenceApart)
{
    write(remote_ / "brief.txt", "hallo");
    fs::create_directories(remote_ / "ordner");
    const auto file = webdav::stat(account_, "/brief.txt");
    const auto directory = webdav::stat(account_, "/ordner");
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(directory.has_value());
    EXPECT_FALSE(file->is_directory);
    EXPECT_EQ(file->size, 5u);
    EXPECT_EQ(file->mtime, util::to_time_t(fs::last_write_time(remote_ / "brief.txt")));
    EXPECT_TRUE(directory->is_directory);
    EXPECT_FALSE(webdav::stat(account_, "/fehlt.txt").has_value());
}

TEST_F(WebDavTest, ListNamesTheEntriesButNotTheDirectoryItself)  // [WebDavTests.md#difficult-names]
{
    write(remote_ / "ordner" / "mit leer zeichen.txt", "a");
    write(remote_ / "ordner" / "Gr\xC3\xB6\xC3\x9F" "e 100%.txt", "bb");
    fs::create_directories(remote_ / "ordner" / "unter");
    std::vector<webdav::Entry> entries = webdav::list(account_, "/ordner");
    std::ranges::sort(entries, {}, &webdav::Entry::name);
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].name, "Gr\xC3\xB6\xC3\x9F" "e 100%.txt");
    EXPECT_EQ(entries[0].size, 2u);
    EXPECT_EQ(entries[1].name, "mit leer zeichen.txt");
    EXPECT_EQ(entries[2].name, "unter");
    EXPECT_TRUE(entries[2].is_directory);
    EXPECT_TRUE(webdav::list(account_, "/ordner/unter").empty());
}

TEST_F(WebDavTest, UploadAndDownloadKeepContentAndTime)
{
    write(local_ / "bild.bin", std::string(300000, 'x') + "ende");
    set_age(local_ / "bild.bin", std::chrono::hours(50));
    EXPECT_EQ(webdav::upload(local_ / "bild.bin", account_, "/bild kopie.bin"), CopyResult::OK);
    EXPECT_EQ(content_of(remote_ / "bild kopie.bin"), content_of(local_ / "bild.bin"));
    EXPECT_EQ(util::to_time_t(fs::last_write_time(remote_ / "bild kopie.bin")), util::to_time_t(fs::last_write_time(local_ / "bild.bin")));

    EXPECT_EQ(webdav::download(account_, "/bild kopie.bin", local_ / "zurueck.bin"), CopyResult::OK);
    EXPECT_EQ(content_of(local_ / "zurueck.bin"), content_of(local_ / "bild.bin"));
    EXPECT_EQ(util::to_time_t(fs::last_write_time(local_ / "zurueck.bin")), util::to_time_t(fs::last_write_time(local_ / "bild.bin")));
}

TEST_F(WebDavTest, EmptyFileTravelsBothWays)
{
    write(local_ / "leer.txt", "");
    EXPECT_EQ(webdav::upload(local_ / "leer.txt", account_, "/leer.txt"), CopyResult::OK);
    EXPECT_TRUE(fs::is_empty(remote_ / "leer.txt"));
    EXPECT_EQ(webdav::download(account_, "/leer.txt", local_ / "zurueck.txt"), CopyResult::OK);
    EXPECT_TRUE(fs::is_empty(local_ / "zurueck.txt"));
}

TEST_F(WebDavTest, MakeDirectoryAcceptsAnExistingDirectoryButNotAFile)
{
    webdav::make_directory(account_, "/neu");
    EXPECT_TRUE(fs::is_directory(remote_ / "neu"));
    EXPECT_NO_THROW(webdav::make_directory(account_, "/neu"));
    write(remote_ / "datei", "x");
    EXPECT_THROW(webdav::make_directory(account_, "/datei"), webdav::Error);
}

TEST_F(WebDavTest, MoveNeverReplacesAnExistingFile)
{
    write(remote_ / "alt.txt", "alt");
    write(remote_ / "besetzt.txt", "besetzt");
    webdav::move(account_, "/alt.txt", "/neu.txt");
    EXPECT_EQ(content_of(remote_ / "neu.txt"), "alt");
    EXPECT_THROW(webdav::move(account_, "/neu.txt", "/besetzt.txt"), webdav::Error);
    EXPECT_EQ(content_of(remote_ / "besetzt.txt"), "besetzt");
}

TEST_F(WebDavTest, RemoveDeletesAndReportsAMissingFile)
{
    write(remote_ / "weg.txt", "x");
    webdav::remove(account_, "/weg.txt");
    EXPECT_FALSE(fs::exists(remote_ / "weg.txt"));
    EXPECT_THROW(webdav::remove(account_, "/weg.txt"), webdav::Error);
}

TEST_F(WebDavTest, WrongPasswordIsAnErrorNotAnAbsence)  // [WebDavTests.md#WrongPasswordIsAnErrorNotAnAbsence]
{
    write(remote_ / "brief.txt", "hallo");
    account_.set_password("falsch");
    EXPECT_THROW(webdav::stat(account_, "/brief.txt"), webdav::Error);
    EXPECT_THROW(webdav::list(account_, "/"), webdav::Error);
}

TEST_F(WebDavTest, ServerThatDoesNotAnswerIsUnreachable)
{
    const std::string url = server_->url();
    server_.reset();
    account_.set_url(url);
    EXPECT_THROW(webdav::stat(account_, "/brief.txt"), SshUnreachable);
}

TEST_F(WebDavTest, UnencryptedAddressOutsideThisComputerIsRefused)  // [WebDavTests.md#UnencryptedAddressOutsideThisComputerIsRefused]
{
    account_.set_url("http://cloud.example.org/dav");
    EXPECT_THROW(webdav::stat(account_, "/brief.txt"), webdav::Error);
    account_.set_url("http://localhost.example.org/dav");
    EXPECT_THROW(webdav::stat(account_, "/brief.txt"), webdav::Error);
}

TEST_F(WebDavTest, RemoteFunctionsReachTheServerThroughTheAccountName)
{
    write(remote_ / "ordner" / "brief.txt", "hallo");
    EXPECT_TRUE(ssh_exists(remote_path("ordner/brief.txt")));
    EXPECT_FALSE(ssh_exists(remote_path("ordner/fehlt.txt")));
    EXPECT_TRUE(ssh_is_directory(remote_path("ordner")));
    EXPECT_FALSE(ssh_is_directory(remote_path("ordner/brief.txt")));
    EXPECT_TRUE(ssh_mkdir_p(remote_path("a/b/c")));
    EXPECT_TRUE(fs::is_directory(remote_ / "a" / "b" / "c"));
    EXPECT_TRUE(ssh_rename(remote_path("ordner/brief.txt"), remote_path("ordner/post.txt")));
    EXPECT_EQ(ssh_list_dir(remote_path("ordner")).size(), 1u);
    EXPECT_TRUE(ssh_remove_file(remote_path("ordner/post.txt")));
    EXPECT_TRUE(ssh_rmdir(remote_path("ordner")));
    EXPECT_FALSE(fs::exists(remote_ / "ordner"));
}

TEST_F(WebDavTest, TreeIsUploadedOnceAndThenLeftAlone)
{
    write(local_ / "oben.txt", "1");
    write(local_ / "tief" / "tiefer" / "unten.txt", "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 2u);
    EXPECT_EQ(content_of(remote_ / "oben.txt"), "1");
    EXPECT_EQ(content_of(remote_ / "tief" / "tiefer" / "unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 0u) << "An unchanged tree was uploaded again.";
}

TEST_F(WebDavTest, ChangedFileIsUploadedAgain)
{
    write(local_ / "bericht.txt", "alt");
    set_age(local_ / "bericht.txt", std::chrono::hours(10));
    ASSERT_EQ(tu::run_sync(upload_item()), 1u);
    write(local_ / "bericht.txt", "neuer");
    EXPECT_EQ(tu::run_sync(upload_item()), 1u);
    EXPECT_EQ(content_of(remote_ / "bericht.txt"), "neuer");
}

TEST_F(WebDavTest, TreeIsDownloaded)
{
    write(remote_ / "oben.txt", "1");
    write(remote_ / "tief" / "unten.txt", "22");
    EXPECT_EQ(tu::run_sync(download_item()), 2u);
    EXPECT_EQ(content_of(local_ / "oben.txt"), "1");
    EXPECT_EQ(content_of(local_ / "tief" / "unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(download_item()), 0u) << "An unchanged tree was downloaded again.";
}

TEST_F(WebDavTest, DeletedSourceFileIsDeletedOnTheServer)
{
    write(local_ / "bleibt.txt", "1");
    write(local_ / "geht.txt", "2");
    SyncItem item = upload_item();
    item.delete_in_target(true);
    ASSERT_EQ(tu::run_sync(item), 2u);
    fs::remove(local_ / "geht.txt");
    tu::run_sync(item);
    EXPECT_TRUE(fs::exists(remote_ / "bleibt.txt"));
    EXPECT_FALSE(fs::exists(remote_ / "geht.txt"));
}

TEST_F(WebDavTest, FatherSonPrincipleKeepsTheReplacedVersionOnTheServer)
{
    write(local_ / "bericht.txt", "alt");
    set_age(local_ / "bericht.txt", std::chrono::hours(10));
    SyncItem item = upload_item();
    item.father_son_principle(true);
    ASSERT_EQ(tu::run_sync(item), 1u);
    write(local_ / "bericht.txt", "neuer");
    ASSERT_EQ(tu::run_sync(item), 1u);
    EXPECT_EQ(content_of(remote_ / "bericht.txt"), "neuer");
    EXPECT_EQ(content_of(remote_ / "bericht.txt.bak.1"), "alt");
}

TEST_F(WebDavTest, TwoWaySyncCarriesChangesAndDeletionsBothWays)  // [WebDavTests.md#two-way]
{
    write(local_ / "lokal.txt", "von hier");
    write(remote_ / "fern.txt", "von dort");
    SyncItem item = upload_item();
    item.two_way_sync(true);
    SyncState state(item.source(), item.target());
    tu::run_sync(item, state);
    EXPECT_EQ(content_of(remote_ / "lokal.txt"), "von hier");
    EXPECT_EQ(content_of(local_ / "fern.txt"), "von dort");
    EXPECT_EQ(tu::run_sync(item, state), 0u) << "A settled two-way pair was copied again.";

    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    fs::remove(local_ / "fern.txt");
    fs::remove(remote_ / "lokal.txt");
    tu::run_sync(item, state);
    EXPECT_FALSE(fs::exists(remote_ / "fern.txt")) << "The local deletion did not reach the server.";
    EXPECT_FALSE(fs::exists(local_ / "lokal.txt")) << "The deletion on the server did not come back.";
}

TEST_F(PlainWebDavTest, ServerWithoutTimeSupportStillSettlesOneWay)  // [WebDavTests.md#PlainWebDavTest]
{
    write(local_ / "bericht.txt", "alt");
    set_age(local_ / "bericht.txt", std::chrono::hours(10));
    ASSERT_EQ(tu::run_sync(upload_item()), 1u);
    EXPECT_EQ(tu::run_sync(upload_item()), 0u) << "The file was uploaded again although nothing changed.";
    write(local_ / "bericht.txt", "neuer");
    EXPECT_EQ(tu::run_sync(upload_item()), 1u);
    EXPECT_EQ(content_of(remote_ / "bericht.txt"), "neuer");
}

TEST_F(PlainWebDavTest, ServerWithoutTimeSupportStillSettlesTwoWay)  // [WebDavTests.md#PlainWebDavTest]
{
    write(local_ / "bericht.txt", "alt");
    set_age(local_ / "bericht.txt", std::chrono::hours(10));
    SyncItem item = upload_item();
    item.two_way_sync(true);
    SyncState state(item.source(), item.target());
    ASSERT_EQ(tu::run_sync(item, state), 1u);
    EXPECT_EQ(tu::run_sync(item, state), 0u) << "The upload time on the server was taken for a change.";
    EXPECT_EQ(content_of(local_ / "bericht.txt"), "alt");
}

}
