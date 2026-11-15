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
#include "S3.h"
#include "Config.h"
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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

const std::string ACCOUNT_NAME = "s3test";
const std::string BUCKET = tu::S3TestServer::BUCKET;

class S3Test : public ::testing::Test {  // [S3Tests.md#S3Test]
protected:
    fs::path local_;
    std::unique_ptr<tu::S3TestServer> server_;
    SshAccount account_;

    virtual int page_size() const { return 0; }

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        local_ = fs::temp_directory_path() / ("autofilesync_s3test_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(local_);
        fs::create_directories(local_);
        server_ = std::make_unique<tu::S3TestServer>(page_size());
        if (!server_->running())
            GTEST_SKIP() << "The S3 test server could not be started (needs python3).";
        account_.set_name(ACCOUNT_NAME);
        account_.set_s3(true);
        account_.set_url(server_->url());
        account_.set_region(tu::S3TestServer::REGION);
        account_.set_user(tu::S3TestServer::ACCESS_KEY);
        account_.set_password(tu::S3TestServer::SECRET_KEY);
        Config::instance().add(account_);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
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

    void put_on_server(const std::string& key, const std::string& content) const
    {
        http::Request request(server_->url() + "/__test/files/" + BUCKET + "/" + key, "PUT");
        request.body(content);
        request.expect_success(request.perform());
    }

    std::map<std::string, std::string> on_server() const  // [S3Tests.md#on_server]
    {
        http::Request request(server_->url() + "/__test/files", "GET");
        request.expect_success(request.perform());
        boost::property_tree::ptree files;
        std::istringstream json(request.response());
        boost::property_tree::read_json(json, files);
        std::map<std::string, std::string> found;
        for (const auto& [key, file] : files)
            found[file.get<std::string>("path")] = file.get<std::string>("content");
        return found;
    }

    static std::string in_bucket(const std::string& key)
    {
        return "/" + BUCKET + "/" + key;
    }

    std::wstring remote_path(const std::string& key = "") const
    {
        return util::string_to_wstring(ACCOUNT_NAME + ":" + in_bucket(key));
    }

    SyncItem upload_item() const
    {
        return SyncItem(L"Hoch", SyncItemType::Tree, local_.wstring(), remote_path("daten"), L"*");
    }
};

class PagedS3Test : public S3Test {  // [S3Tests.md#PagedS3Test]
protected:
    int page_size() const override { return 2; }
};

TEST_F(S3Test, SignatureIsCheckedByAnIndependentImplementation)  // [S3Tests.md#signature]
{
    http::Request request(server_->url() + "/__test/botocore", "GET");
    request.expect_success(request.perform());
    EXPECT_FALSE(s3::list(account_, "/").empty());
    if (request.response() != "yes")
        GTEST_SKIP() << "botocore is not installed; the signatures were checked by the test server alone.";
}

TEST_F(S3Test, WrongSecretRegionOrKeyAreErrorsNotAbsence)  // [S3Tests.md#refused]
{
    put_on_server("brief.txt", "hallo");
    SshAccount wrong_secret = account_;
    wrong_secret.set_password("falsch");
    EXPECT_THROW(s3::stat(wrong_secret, in_bucket("brief.txt")), http::Error);
    EXPECT_THROW(s3::list(wrong_secret, in_bucket("")), http::Error);
    SshAccount wrong_region = account_;
    wrong_region.set_region("us-east-1");
    EXPECT_THROW(s3::list(wrong_region, "/"), http::Error);
    SshAccount wrong_key = account_;
    wrong_key.set_user("AKIAUNBEKANNT");
    EXPECT_THROW(s3::list(wrong_key, "/"), http::Error);
}

TEST_F(S3Test, RootListsTheBuckets)
{
    std::vector<s3::Entry> buckets = s3::list(account_, "/");
    ASSERT_EQ(buckets.size(), 2u);
    EXPECT_TRUE(buckets[0].is_directory);
    EXPECT_TRUE(s3::stat(account_, "/")->is_directory);
    EXPECT_TRUE(s3::stat(account_, "/" + BUCKET)->is_directory);
    EXPECT_FALSE(s3::stat(account_, "/gibtesnicht").has_value());
}

TEST_F(S3Test, StatTellsFileFolderAndAbsenceApart)  // [S3Tests.md#folders]
{
    put_on_server("ordner/brief.txt", "hallo");
    const auto file = s3::stat(account_, in_bucket("ordner/brief.txt"));
    const auto folder = s3::stat(account_, in_bucket("ordner"));
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(folder.has_value());
    EXPECT_FALSE(file->is_directory);
    EXPECT_EQ(file->size, 5u);
    EXPECT_TRUE(folder->is_directory);
    EXPECT_FALSE(s3::stat(account_, in_bucket("ordner/fehlt.txt")).has_value());
    EXPECT_FALSE(s3::stat(account_, in_bucket("ord")).has_value()) << "A name that only begins like a folder was taken for it.";
}

TEST_F(S3Test, ListShowsFilesAndFoldersOfOneLevel)
{
    put_on_server("ordner/a.txt", "1");
    put_on_server("ordner/tief/b.txt", "22");
    put_on_server("ordner/tief/tiefer/c.txt", "333");
    std::vector<s3::Entry> entries = s3::list(account_, in_bucket("ordner"));
    std::ranges::sort(entries, {}, &s3::Entry::name);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].name, "a.txt");
    EXPECT_EQ(entries[0].size, 1u);
    EXPECT_EQ(entries[1].name, "tief");
    EXPECT_TRUE(entries[1].is_directory);
}

TEST_F(PagedS3Test, ListFollowsEveryPage)
{
    for (const char* name : {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt"})
        put_on_server(std::string("viele/") + name, "x");
    EXPECT_EQ(s3::list(account_, in_bucket("viele")).size(), 5u);
}

TEST_F(S3Test, NamesWithBlanksUmlautsAndSignsSurvive)  // [S3Tests.md#difficult-names]
{
    const std::string name = "Anna's Gr\xC3\xB6\xC3\x9F" "e & 100% (neu) +1 =2.txt";
    write(local_ / "quelle.txt", "inhalt");
    ASSERT_EQ(s3::upload(local_ / "quelle.txt", account_, in_bucket("ordner mit leer/" + name)), CopyResult::OK);
    const auto entry = s3::stat(account_, in_bucket("ordner mit leer/" + name));
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->name, name);
    EXPECT_EQ(on_server().at(BUCKET + "/ordner mit leer/" + name), "inhalt");
    ASSERT_EQ(s3::list(account_, in_bucket("ordner mit leer")).size(), 1u);
    EXPECT_EQ(s3::list(account_, in_bucket("ordner mit leer"))[0].name, name);
}

TEST_F(S3Test, UploadAndDownloadKeepContentAndTime)  // [S3Tests.md#time]
{
    write(local_ / "bericht.txt", "erste fassung");
    fs::last_write_time(local_ / "bericht.txt", fs::file_time_type::clock::now() - std::chrono::hours(50));
    ASSERT_EQ(s3::upload(local_ / "bericht.txt", account_, in_bucket("bericht.txt")), CopyResult::OK);
    EXPECT_EQ(s3::stat(account_, in_bucket("bericht.txt"))->mtime, util::to_time_t(fs::last_write_time(local_ / "bericht.txt")));
    ASSERT_EQ(s3::download(account_, in_bucket("bericht.txt"), local_ / "zurueck.txt"), CopyResult::OK);
    EXPECT_EQ(content_of(local_ / "zurueck.txt"), "erste fassung");
    EXPECT_EQ(util::to_time_t(fs::last_write_time(local_ / "zurueck.txt")), util::to_time_t(fs::last_write_time(local_ / "bericht.txt")));
}

TEST_F(S3Test, LargeFileIsUploadedInParts)  // [S3Tests.md#LargeFileIsUploadedInParts]
{
    const std::string content = std::string(33 * 1024 * 1024, 'a') + "ende";
    write(local_ / "gross.bin", content);
    fs::last_write_time(local_ / "gross.bin", fs::file_time_type::clock::now() - std::chrono::hours(5));
    ASSERT_EQ(s3::upload(local_ / "gross.bin", account_, in_bucket("gross.bin")), CopyResult::OK);
    const auto entry = s3::stat(account_, in_bucket("gross.bin"));
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->size, content.size());
    EXPECT_EQ(entry->mtime, util::to_time_t(fs::last_write_time(local_ / "gross.bin")));
    ASSERT_EQ(s3::download(account_, in_bucket("gross.bin"), local_ / "zurueck.bin"), CopyResult::OK);
    EXPECT_EQ(content_of(local_ / "zurueck.bin"), content);
}

TEST_F(S3Test, EmptyFileTravelsBothWays)
{
    write(local_ / "leer.txt", "");
    ASSERT_EQ(s3::upload(local_ / "leer.txt", account_, in_bucket("leer.txt")), CopyResult::OK);
    EXPECT_EQ(s3::stat(account_, in_bucket("leer.txt"))->size, 0u);
    ASSERT_EQ(s3::download(account_, in_bucket("leer.txt"), local_ / "zurueck.txt"), CopyResult::OK);
    EXPECT_TRUE(fs::is_empty(local_ / "zurueck.txt"));
}

TEST_F(S3Test, RemoveDeletesAndReportsAMissingFile)
{
    put_on_server("weg.txt", "x");
    s3::remove(account_, in_bucket("weg.txt"));
    EXPECT_TRUE(on_server().empty());
    EXPECT_THROW(s3::remove(account_, in_bucket("weg.txt")), http::Error);
}

TEST_F(S3Test, MoveCopiesWithItsTimeAndNeverReplaces)  // [S3Tests.md#move]
{
    write(local_ / "alt.txt", "alt");
    fs::last_write_time(local_ / "alt.txt", fs::file_time_type::clock::now() - std::chrono::hours(9));
    ASSERT_EQ(s3::upload(local_ / "alt.txt", account_, in_bucket("a/alt.txt")), CopyResult::OK);
    put_on_server("b/besetzt.txt", "besetzt");
    s3::move(account_, in_bucket("a/alt.txt"), in_bucket("b/neu name.txt"));
    EXPECT_EQ(on_server().at(BUCKET + "/b/neu name.txt"), "alt");
    EXPECT_EQ(on_server().count(BUCKET + "/a/alt.txt"), 0u);
    EXPECT_EQ(s3::stat(account_, in_bucket("b/neu name.txt"))->mtime, util::to_time_t(fs::last_write_time(local_ / "alt.txt")));
    EXPECT_THROW(s3::move(account_, in_bucket("b/neu name.txt"), in_bucket("b/besetzt.txt")), http::Error);
    EXPECT_EQ(on_server().at(BUCKET + "/b/besetzt.txt"), "besetzt");
}

TEST_F(S3Test, FolderIsAMarkerThatCanBeMadeAndRemoved)  // [S3Tests.md#folders]
{
    s3::make_directory(account_, in_bucket("neu"));
    EXPECT_NO_THROW(s3::make_directory(account_, in_bucket("neu")));
    EXPECT_TRUE(s3::stat(account_, in_bucket("neu"))->is_directory);
    EXPECT_TRUE(s3::list(account_, in_bucket("neu")).empty());
    s3::remove(account_, in_bucket("neu"));
    EXPECT_FALSE(s3::stat(account_, in_bucket("neu")).has_value());
    put_on_server("datei", "x");
    EXPECT_THROW(s3::make_directory(account_, in_bucket("datei")), http::Error);
    EXPECT_THROW(s3::make_directory(account_, "/neuer-bucket"), http::Error) << "Buckets are not created.";
}

TEST_F(S3Test, RemoteFunctionsReachTheBucketThroughTheAccountName)
{
    put_on_server("ordner/brief.txt", "hallo");
    EXPECT_TRUE(ssh_exists(remote_path("ordner/brief.txt")));
    EXPECT_TRUE(ssh_is_directory(remote_path("ordner")));
    EXPECT_TRUE(ssh_mkdir_p(remote_path("a/b/c")));
    EXPECT_TRUE(ssh_is_directory(remote_path("a/b")));
    EXPECT_TRUE(ssh_rename(remote_path("ordner/brief.txt"), remote_path("ordner/post.txt")));
    EXPECT_EQ(ssh_list_dir(remote_path("ordner")).size(), 1u);
    EXPECT_TRUE(ssh_remove_file(remote_path("ordner/post.txt")));
    EXPECT_FALSE(ssh_exists(remote_path("ordner/post.txt")));
}

TEST_F(S3Test, TreeIsUploadedOnceAndThenLeftAlone)
{
    write(local_ / "oben.txt", "1");
    write(local_ / "tief" / "tiefer" / "unten.txt", "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 2u);
    const auto files = on_server();
    EXPECT_EQ(files.at(BUCKET + "/daten/oben.txt"), "1");
    EXPECT_EQ(files.at(BUCKET + "/daten/tief/tiefer/unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 0u) << "An unchanged tree was uploaded again.";
}

TEST_F(S3Test, TreeIsDownloaded)
{
    put_on_server("daten/oben.txt", "1");
    put_on_server("daten/tief/unten.txt", "22");
    const SyncItem item(L"Herunter", SyncItemType::Tree, remote_path("daten"), local_.wstring(), L"*");
    EXPECT_EQ(tu::run_sync(item), 2u);
    EXPECT_EQ(content_of(local_ / "oben.txt"), "1");
    EXPECT_EQ(content_of(local_ / "tief" / "unten.txt"), "22");
}

TEST_F(S3Test, ChangedFileDeletionAndEarlierVersionReachTheBucket)
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
    EXPECT_EQ(files.at(BUCKET + "/daten/bericht.txt"), "neuer");
    EXPECT_EQ(files.at(BUCKET + "/daten/bericht.txt.bak.1"), "alt");
    EXPECT_EQ(files.count(BUCKET + "/daten/geht.txt"), 0u);
}

TEST_F(S3Test, TwoWaySyncCarriesChangesAndDeletionsBothWays)
{
    write(local_ / "lokal.txt", "von hier");
    put_on_server("daten/fern.txt", "von dort");
    SyncItem item = upload_item();
    item.two_way_sync(true);
    SyncState state(item.source(), item.target());
    tu::run_sync(item, state);
    EXPECT_EQ(on_server().at(BUCKET + "/daten/lokal.txt"), "von hier");
    EXPECT_EQ(content_of(local_ / "fern.txt"), "von dort");
    EXPECT_EQ(tu::run_sync(item, state), 0u) << "A settled two-way pair was copied again.";

    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    fs::remove(local_ / "fern.txt");
    s3::remove(account_, in_bucket("daten/lokal.txt"));
    tu::run_sync(item, state);
    EXPECT_EQ(on_server().count(BUCKET + "/daten/fern.txt"), 0u) << "The local deletion did not reach the bucket.";
    EXPECT_FALSE(fs::exists(local_ / "lokal.txt")) << "The deletion in the bucket did not come back.";
}

}
