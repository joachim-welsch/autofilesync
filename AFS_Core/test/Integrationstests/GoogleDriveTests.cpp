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
#include "GoogleDrive.h"
#include "Config.h"
#include "GoogleAuth.h"
#include "HttpClient.h"
#include "ScopedDeleteMode.h"
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
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

const std::string ACCOUNT_NAME = "drivetest";

struct ServerFile {
    std::string content;
    std::string modified;
    bool trashed = false;
    bool folder = false;
};

bool wait_for(google_auth::Login& login)
{
    for (int attempt = 0; attempt < 50; ++attempt)
        if (login.finished(std::chrono::milliseconds(100)))
            return true;
    return false;
}

class GoogleDriveTest : public ::testing::Test {  // [GoogleDriveTests.md#GoogleDriveTest]
protected:
    fs::path local_;
    std::unique_ptr<tu::GoogleDriveTestServer> server_;
    google_auth::Settings real_settings_ = google_auth::settings();
    SshAccount account_;

    virtual int page_size() const { return 0; }

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        local_ = fs::temp_directory_path() / ("autofilesync_drivetest_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(local_);
        fs::create_directories(local_);
        server_ = std::make_unique<tu::GoogleDriveTestServer>(page_size());
        if (!server_->running())
            GTEST_SKIP() << "The Google Drive test server could not be started (needs python3).";
        google_auth::settings() = {tu::GoogleDriveTestServer::CLIENT_ID, tu::GoogleDriveTestServer::CLIENT_SECRET,
            server_->url() + "/auth", server_->url() + "/token", server_->url()};
        account_.set_name(ACCOUNT_NAME + "-" + test_name);  // [GoogleDriveTests.md#account-per-test]
        account_.set_google_drive(true);
        account_.set_url(server_->url());
        account_.set_password(tu::GoogleDriveTestServer::REFRESH_TOKEN);
        Config::instance().add(account_);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        google_auth::settings() = real_settings_;
        server_.reset();
        fs::remove_all(local_);
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

    void put_on_server(const std::string& path, const std::string& content) const
    {
        http::Request request(server_->url() + "/__test/files/" + path, "PUT");
        request.body(content);
        request.expect_success(request.perform());
    }

    std::map<std::string, ServerFile> on_server() const  // [GoogleDriveTests.md#on_server]
    {
        http::Request request(server_->url() + "/__test/files", "GET");
        request.expect_success(request.perform());
        boost::property_tree::ptree files;
        std::istringstream json(request.response());
        boost::property_tree::read_json(json, files);
        std::map<std::string, ServerFile> found;
        for (const auto& [key, file] : files)
            if (!file.get<bool>("trashed"))
                found[file.get<std::string>("path")] = {file.get<std::string>("content"), file.get<std::string>("modifiedTime"), false, file.get<bool>("folder")};
        return found;
    }

    std::wstring remote_path(const std::string& path = "") const
    {
        return util::string_to_wstring(account_.name() + ":/" + path);
    }

    SyncItem upload_item() const
    {
        return SyncItem(L"Hoch", SyncItemType::Tree, local_.wstring(), remote_path("sicherung"), L"*");
    }
};

class PagedGoogleDriveTest : public GoogleDriveTest {  // [GoogleDriveTests.md#PagedGoogleDriveTest]
protected:
    int page_size() const override { return 2; }
};

TEST_F(GoogleDriveTest, LoginThroughTheBrowserYieldsARefreshToken)  // [GoogleDriveTests.md#login]
{
    google_auth::Login login;
    std::thread browser(tu::visit_like_a_browser, login.browser_url());
    const bool finished = wait_for(login);
    browser.join();
    ASSERT_TRUE(finished) << "The redirect of the login never arrived.";
    EXPECT_EQ(login.refresh_token(), tu::GoogleDriveTestServer::REFRESH_TOKEN);
}

TEST_F(GoogleDriveTest, LoginRefusedByTheUserFails)
{
    google_auth::Login login;
    const std::string url = login.browser_url();
    const std::string redirect = http::unescaped(url.substr(url.find("redirect_uri=") + 13, url.find('&', url.find("redirect_uri=")) - url.find("redirect_uri=") - 13));
    std::thread browser(tu::visit_like_a_browser, redirect + "/?error=access_denied&state=fremd");
    const bool finished = wait_for(login);
    browser.join();
    ASSERT_TRUE(finished);
    EXPECT_THROW(login.refresh_token(), http::Error);
}

TEST_F(GoogleDriveTest, RevokedLoginIsAnErrorNotAnAbsence)
{
    put_on_server("brief.txt", "hallo");
    account_.set_password("widerrufen");
    EXPECT_THROW(google_drive::stat(account_, "/brief.txt"), http::Error);
}

TEST_F(GoogleDriveTest, StatTellsFileFolderAndAbsenceApart)
{
    put_on_server("ordner/brief.txt", "hallo");
    const auto file = google_drive::stat(account_, "/ordner/brief.txt");
    const auto folder = google_drive::stat(account_, "/ordner");
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(folder.has_value());
    EXPECT_FALSE(file->is_directory);
    EXPECT_EQ(file->size, 5u);
    EXPECT_TRUE(folder->is_directory);
    EXPECT_FALSE(google_drive::stat(account_, "/ordner/fehlt.txt").has_value());
    EXPECT_FALSE(google_drive::stat(account_, "/fehlt/brief.txt").has_value());
    EXPECT_TRUE(google_drive::stat(account_, "/")->is_directory);
}

TEST_F(PagedGoogleDriveTest, ListFollowsEveryPage)
{
    for (const char* name : {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt"})
        put_on_server(std::string("viele/") + name, "x");
    EXPECT_EQ(google_drive::list(account_, "/viele").size(), 5u);
}

TEST_F(GoogleDriveTest, NamesWithQuotesAndUmlautsSurvive)  // [GoogleDriveTests.md#difficult-names]
{
    const std::string name = "Anna's \"Gr\xC3\xB6\xC3\x9F" "e\" \\ 100%.txt";
    write(local_ / "quelle.txt", "inhalt");
    google_drive::make_directory(account_, "/ordner");
    ASSERT_EQ(google_drive::upload(local_ / "quelle.txt", account_, "/ordner/" + name), CopyResult::OK);
    const auto entry = google_drive::stat(account_, "/ordner/" + name);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->name, name);
    EXPECT_EQ(on_server().at("ordner/" + name).content, "inhalt");
}

TEST_F(GoogleDriveTest, UploadCreatesThenReplacesAndKeepsTheTime)
{
    write(local_ / "bericht.txt", "erste fassung");
    fs::last_write_time(local_ / "bericht.txt", fs::file_time_type::clock::now() - std::chrono::hours(50));
    ASSERT_EQ(google_drive::upload(local_ / "bericht.txt", account_, "/bericht.txt"), CopyResult::OK);
    EXPECT_EQ(google_drive::stat(account_, "/bericht.txt")->mtime, util::to_time_t(fs::last_write_time(local_ / "bericht.txt")));
    write(local_ / "bericht.txt", "zweite");
    ASSERT_EQ(google_drive::upload(local_ / "bericht.txt", account_, "/bericht.txt"), CopyResult::OK);
    const auto files = on_server();
    EXPECT_EQ(files.size(), 1u) << "The second upload made a second file of the same name.";
    EXPECT_EQ(files.at("bericht.txt").content, "zweite");
}

TEST_F(GoogleDriveTest, DownloadKeepsContentAndTime)
{
    write(local_ / "bild.bin", std::string(200000, 'x') + "ende");
    fs::last_write_time(local_ / "bild.bin", fs::file_time_type::clock::now() - std::chrono::hours(30));
    ASSERT_EQ(google_drive::upload(local_ / "bild.bin", account_, "/bild.bin"), CopyResult::OK);
    ASSERT_EQ(google_drive::download(account_, "/bild.bin", local_ / "zurueck.bin"), CopyResult::OK);
    EXPECT_EQ(content_of(local_ / "zurueck.bin"), content_of(local_ / "bild.bin"));
    EXPECT_EQ(util::to_time_t(fs::last_write_time(local_ / "zurueck.bin")), util::to_time_t(fs::last_write_time(local_ / "bild.bin")));
}

TEST_F(GoogleDriveTest, RemovePutsTheFileIntoTheTrashOfTheDrive)  // [GoogleDriveTests.md#trash]
{
    put_on_server("weg.txt", "x");
    google_drive::remove(account_, "/weg.txt");
    EXPECT_FALSE(google_drive::stat(account_, "/weg.txt").has_value());
    EXPECT_TRUE(on_server().empty());
    EXPECT_THROW(google_drive::remove(account_, "/weg.txt"), http::Error);
}

TEST_F(GoogleDriveTest, MoveRenamesMovesAndNeverReplaces)
{
    put_on_server("a/alt.txt", "alt");
    put_on_server("b/besetzt.txt", "besetzt");
    google_drive::move(account_, "/a/alt.txt", "/a/neu.txt");
    google_drive::move(account_, "/a/neu.txt", "/b/neu.txt");
    EXPECT_EQ(on_server().at("b/neu.txt").content, "alt");
    EXPECT_THROW(google_drive::move(account_, "/b/neu.txt", "/b/besetzt.txt"), http::Error);
    EXPECT_EQ(on_server().at("b/besetzt.txt").content, "besetzt");
}

TEST_F(GoogleDriveTest, MakeDirectoryAcceptsAnExistingFolderButNotAFile)
{
    google_drive::make_directory(account_, "/neu");
    EXPECT_NO_THROW(google_drive::make_directory(account_, "/neu"));
    EXPECT_EQ(on_server().size(), 1u);
    put_on_server("datei", "x");
    EXPECT_THROW(google_drive::make_directory(account_, "/datei"), http::Error);
}

TEST_F(GoogleDriveTest, RemoteFunctionsReachTheDriveThroughTheAccountName)
{
    put_on_server("ordner/brief.txt", "hallo");
    EXPECT_TRUE(ssh_exists(remote_path("ordner/brief.txt")));
    EXPECT_TRUE(ssh_is_directory(remote_path("ordner")));
    EXPECT_TRUE(ssh_mkdir_p(remote_path("a/b/c")));
    EXPECT_TRUE(ssh_rename(remote_path("ordner/brief.txt"), remote_path("ordner/post.txt")));
    EXPECT_EQ(ssh_list_dir(remote_path("ordner")).size(), 1u);
    EXPECT_TRUE(ssh_remove_file(remote_path("ordner/post.txt")));
    EXPECT_FALSE(ssh_exists(remote_path("ordner/post.txt")));
}

TEST_F(GoogleDriveTest, TreeIsUploadedOnceAndThenLeftAlone)
{
    write(local_ / "oben.txt", "1");
    write(local_ / "tief" / "tiefer" / "unten.txt", "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 2u);
    const auto files = on_server();
    EXPECT_EQ(files.at("sicherung/oben.txt").content, "1");
    EXPECT_EQ(files.at("sicherung/tief/tiefer/unten.txt").content, "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 0u) << "An unchanged tree was uploaded again.";
}

TEST_F(GoogleDriveTest, TreeIsDownloaded)
{
    put_on_server("sicherung/oben.txt", "1");
    put_on_server("sicherung/tief/unten.txt", "22");
    const SyncItem item(L"Herunter", SyncItemType::Tree, remote_path("sicherung"), local_.wstring(), L"*");
    EXPECT_EQ(tu::run_sync(item), 2u);
    EXPECT_EQ(content_of(local_ / "oben.txt"), "1");
    EXPECT_EQ(content_of(local_ / "tief" / "unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(item), 0u) << "An unchanged tree was downloaded again.";
}

TEST_F(GoogleDriveTest, ChangedFileDeletionAndEarlierVersionReachTheDrive)
{
    write(local_ / "bericht.txt", "alt");
    write(local_ / "geht.txt", "2");
    fs::last_write_time(local_ / "bericht.txt", fs::file_time_type::clock::now() - std::chrono::hours(10));
    SyncItem item = upload_item();
    item.delete_in_target(true);
    item.father_son_principle(true);
    ASSERT_EQ(tu::run_sync(item), 2u);
    write(local_ / "bericht.txt", "neuer");
    fs::remove(local_ / "geht.txt");
    tu::run_sync(item);
    const auto files = on_server();
    EXPECT_EQ(files.at("sicherung/bericht.txt").content, "neuer");
    EXPECT_EQ(files.at("sicherung/bericht.txt.bak.1").content, "alt");
    EXPECT_EQ(files.count("sicherung/geht.txt"), 0u);
}

TEST_F(GoogleDriveTest, TwoWaySyncCarriesChangesAndDeletionsBothWays)
{
    write(local_ / "lokal.txt", "von hier");
    put_on_server("sicherung/fern.txt", "von dort");
    SyncItem item = upload_item();
    item.two_way_sync(true);
    SyncState state(item.source(), item.target());
    tu::run_sync(item, state);
    EXPECT_EQ(on_server().at("sicherung/lokal.txt").content, "von hier");
    EXPECT_EQ(content_of(local_ / "fern.txt"), "von dort");
    EXPECT_EQ(tu::run_sync(item, state), 0u) << "A settled two-way pair was copied again.";

    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    fs::remove(local_ / "fern.txt");
    google_drive::remove(account_, "/sicherung/lokal.txt");
    tu::run_sync(item, state);
    EXPECT_EQ(on_server().count("sicherung/fern.txt"), 0u) << "The local deletion did not reach the drive.";
    EXPECT_FALSE(fs::exists(local_ / "lokal.txt")) << "The deletion in the drive did not come back.";
}

}
