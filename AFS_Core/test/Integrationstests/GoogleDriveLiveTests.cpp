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
#include "PasswordCrypto.h"
#include "PercentEncoding.h"
#include "ScopedDeleteMode.h"
#include "SyncFileSsh.h"
#include "SyncItem.h"
#include "SyncRunner.h"
#include "SyncState.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "StrUtil.h"
#include "FileTime.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace AFS_Core;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;

namespace {

const std::string USER_ACCOUNT_NAME = "Testuser";  // [GoogleDriveLiveTests.md#account-of-the-user]
const std::string TEST_ACCOUNT_NAME = "drivelive";
constexpr char CONFIG_FILE[] = "autofilesync.json";
constexpr char KEY_FILE[] = "secret.key";

fs::path user_config_dir()  // [GoogleDriveLiveTests.md#user_config_dir]
{
    const char* dir = std::getenv(TestPaths::USER_CONFIG_DIR_VARIABLE);
    return dir != nullptr ? fs::path(dir) : fs::path(Config::get_root_pathname()).parent_path();
}

std::optional<std::string> refresh_token_of(const std::string& stored, const fs::path& key_file)
{
    if (!passwordcrypto::is_encrypted(stored))
        return stored;
    if (!fs::exists(key_file))  // decrypt() would create a key in the directory of the user
        return std::nullopt;
    return passwordcrypto::decrypt(stored, key_file);
}

std::optional<SshAccount> test_account(const pt::ptree& stored, const fs::path& key_file)
{
    const auto token = refresh_token_of(stored.get<std::string>("password", ""), key_file);
    if (!token || token->empty())
        return std::nullopt;
    SshAccount account;
    account.set_name(TEST_ACCOUNT_NAME);
    account.set_google_drive(true);
    account.set_url(stored.get<std::string>("url", ""));
    account.set_password(*token);
    return account;
}

std::optional<SshAccount> account_of_the_user()  // [GoogleDriveLiveTests.md#account-of-the-user]
{
    const fs::path dir = user_config_dir();
    pt::ptree config;
    try {
        pt::read_json((dir / CONFIG_FILE).string(), config);
    } catch (const pt::json_parser_error&) {
        return std::nullopt;
    }
    const auto accounts = config.get_child_optional("SshAccounts");
    if (!accounts)
        return std::nullopt;
    for (const auto& [key, stored] : *accounts)
        if (stored.get<std::string>("name", "") == USER_ACCOUNT_NAME && stored.get<bool>("googleDrive", false))
            return test_account(stored, dir / KEY_FILE);
    return std::nullopt;
}

struct LiveDrive {
    std::optional<SshAccount> account;
    std::string unavailable;
};

LiveDrive signed_in_drive()
{
    if (!google_auth::configured())
        return {std::nullopt, "this build has no Google client (google-client.cmake)."};
    const auto account = account_of_the_user();
    if (!account)
        return {std::nullopt, "no Google Drive account \"" + USER_ACCOUNT_NAME + "\" with a readable sign-in in " + (user_config_dir() / CONFIG_FILE).string() + "."};
    try {
        google_auth::access_token(*account);
    } catch (const std::exception& e) {
        return {std::nullopt, e.what()};
    }
    return {account, ""};
}

const LiveDrive& live_drive()  // [GoogleDriveLiveTests.md#live_drive]
{
    static const LiveDrive DRIVE = signed_in_drive();
    return DRIVE;
}

struct DriveFile {
    std::string id;
    bool trashed = false;
};

class GoogleDriveLiveTest : public ::testing::Test {  // [GoogleDriveLiveTests.md#GoogleDriveLiveTest]
protected:
    fs::path local_;
    fs::path source_;
    std::string root_;
    SshAccount account_;

    void SetUp() override
    {
        if (!live_drive().account)
            GTEST_SKIP() << "No test against the real Google Drive: " << live_drive().unavailable;
        const std::string unique = std::string(testing::UnitTest::GetInstance()->current_test_info()->name()) + "_" + std::to_string(TestPaths::process_id());
        local_ = fs::temp_directory_path() / ("autofilesync_drivelive_" + unique);
        source_ = local_ / "quelle";
        fs::remove_all(local_);
        fs::create_directories(source_);
        account_ = *live_drive().account;
        Config::instance().add(account_);
        Synchronize::start();
        root_ = "/autofilesync_test_" + unique;  // [GoogleDriveLiveTests.md#folder-per-test]
        google_drive::make_directory(account_, root_);
    }

    void TearDown() override
    {
        if (root_.empty())
            return;
        EXPECT_NO_THROW(delete_for_good(root_.substr(1)));  // [GoogleDriveLiveTests.md#delete_for_good]
        Config::instance().reset();
        fs::remove_all(local_);
    }

    std::string files_url() const
    {
        return account_.url() + "/drive/v3/files";
    }

    std::vector<DriveFile> named(const std::string& name) const  // [GoogleDriveLiveTests.md#named]
    {
        const std::string token = google_auth::access_token(account_);
        http::Request request(files_url() + "?fields=" + percent::encoded("files(id,trashed)") + "&q=" + percent::encoded("name = '" + name + "'"), "GET");
        request.bearer(token);
        request.expect_success(request.perform());
        pt::ptree answer;
        std::istringstream json(request.response());
        pt::read_json(json, answer);
        std::vector<DriveFile> found;
        if (const auto files = answer.get_child_optional("files"))
            for (const auto& [key, file] : *files)
                found.push_back({file.get<std::string>("id"), file.get<bool>("trashed", false)});
        return found;
    }

    void delete_for_good(const std::string& name) const  // [GoogleDriveLiveTests.md#delete_for_good]
    {
        const std::string token = google_auth::access_token(account_);
        for (const DriveFile& file : named(name)) {
            http::Request request(files_url() + "/" + file.id, "DELETE");
            request.bearer(token);
            request.expect_success(request.perform());
        }
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

    std::string in_drive(const std::string& path = "") const
    {
        return path.empty() ? root_ : root_ + "/" + path;
    }

    std::wstring remote_path(const std::string& path = "") const
    {
        return util::string_to_wstring(account_.name() + ":" + in_drive(path));
    }

    void put_on_drive(const std::string& path, const std::string& content) const  // [GoogleDriveLiveTests.md#put_on_drive]
    {
        const fs::path staged = local_ / "ablage" / "hoch.tmp";
        write(staged, content);
        ASSERT_TRUE(ssh_mkdir_p(remote_path(fs::path(path).parent_path().generic_string())));
        ASSERT_EQ(google_drive::upload(staged, account_, in_drive(path)), CopyResult::OK);
    }

    std::string content_on_drive(const std::string& path) const
    {
        const fs::path fetched = local_ / "ablage" / "herunter.tmp";
        fs::create_directories(fetched.parent_path());
        fs::remove(fetched);
        EXPECT_EQ(google_drive::download(account_, in_drive(path), fetched), CopyResult::OK) << path;
        return content_of(fetched);
    }

    bool on_drive(const std::string& path) const
    {
        return google_drive::stat(account_, in_drive(path)).has_value();
    }

    static std::size_t copy_into_folder(const SyncItem& item)  // [GoogleDriveLiveTests.md#single-file]
    {
        [[maybe_unused]] SshSessionScope sessions;
        std::size_t copied = 0;
        SyncFile(item, copied, false, true)();
        return copied;
    }

    SyncItem upload_item() const
    {
        return SyncItem(L"Hoch", SyncItemType::Tree, source_.wstring(), remote_path("sicherung"), L"*");
    }
};

TEST_F(GoogleDriveLiveTest, StatTellsFileFolderAndAbsenceApart)
{
    put_on_drive("ordner/brief.txt", "hallo");
    const auto file = google_drive::stat(account_, in_drive("ordner/brief.txt"));
    const auto folder = google_drive::stat(account_, in_drive("ordner"));
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(folder.has_value());
    EXPECT_FALSE(file->is_directory);
    EXPECT_EQ(file->size, 5u);
    EXPECT_TRUE(folder->is_directory);
    EXPECT_FALSE(on_drive("ordner/fehlt.txt"));
    EXPECT_FALSE(on_drive("fehlt/brief.txt"));
    EXPECT_TRUE(google_drive::stat(account_, "/")->is_directory);
}

TEST_F(GoogleDriveLiveTest, ListShowsFilesAndFoldersOfOneFolderOnly)
{
    put_on_drive("ordner/a.txt", "1");
    put_on_drive("ordner/b.txt", "22");
    put_on_drive("ordner/tief/c.txt", "333");
    const auto entries = google_drive::list(account_, in_drive("ordner"));
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(std::count_if(entries.begin(), entries.end(), [](const google_drive::Entry& entry) { return entry.is_directory; }), 1);
    EXPECT_TRUE(google_drive::list(account_, in_drive("ordner/tief")).size() == 1u);
}

TEST_F(GoogleDriveLiveTest, NamesWithQuotesAndUmlautsSurvive)  // [GoogleDriveLiveTests.md#difficult-names]
{
    const std::string name = "Anna's \"Gr\xC3\xB6\xC3\x9F" "e\" \\ 100%.txt";
    put_on_drive("ordner/" + name, "inhalt");
    const auto entry = google_drive::stat(account_, in_drive("ordner/" + name));
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->name, name);
    EXPECT_EQ(content_on_drive("ordner/" + name), "inhalt");
}

TEST_F(GoogleDriveLiveTest, UploadCreatesThenReplacesAndKeepsTheTime)
{
    write(source_ / "bericht.txt", "erste fassung");
    fs::last_write_time(source_ / "bericht.txt", fs::file_time_type::clock::now() - std::chrono::hours(50));
    ASSERT_EQ(google_drive::upload(source_ / "bericht.txt", account_, in_drive("bericht.txt")), CopyResult::OK);
    EXPECT_EQ(google_drive::stat(account_, in_drive("bericht.txt"))->mtime, file_time::to_time_t(fs::last_write_time(source_ / "bericht.txt")));
    write(source_ / "bericht.txt", "zweite");
    ASSERT_EQ(google_drive::upload(source_ / "bericht.txt", account_, in_drive("bericht.txt")), CopyResult::OK);
    EXPECT_EQ(named("bericht.txt").size(), 1u) << "The second upload made a second file of the same name.";
    EXPECT_EQ(content_on_drive("bericht.txt"), "zweite");
}

TEST_F(GoogleDriveLiveTest, DownloadKeepsContentAndTime)
{
    write(source_ / "bild.bin", std::string(200000, 'x') + "ende");
    fs::last_write_time(source_ / "bild.bin", fs::file_time_type::clock::now() - std::chrono::hours(30));
    ASSERT_EQ(google_drive::upload(source_ / "bild.bin", account_, in_drive("bild.bin")), CopyResult::OK);
    ASSERT_EQ(google_drive::download(account_, in_drive("bild.bin"), local_ / "zurueck.bin"), CopyResult::OK);
    EXPECT_EQ(content_of(local_ / "zurueck.bin"), content_of(source_ / "bild.bin"));
    EXPECT_EQ(file_time::to_time_t(fs::last_write_time(local_ / "zurueck.bin")), file_time::to_time_t(fs::last_write_time(source_ / "bild.bin")));
}

TEST_F(GoogleDriveLiveTest, EmptyFileGoesUpAndComesBack)
{
    put_on_drive("leer.txt", "");
    const auto entry = google_drive::stat(account_, in_drive("leer.txt"));
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->size, 0u);
    EXPECT_EQ(content_on_drive("leer.txt"), "");
}

TEST_F(GoogleDriveLiveTest, RemovePutsTheFileIntoTheTrashOfTheDrive)  // [GoogleDriveLiveTests.md#trash]
{
    const std::string name = "weg_" + root_.substr(1) + ".txt";
    put_on_drive(name, "x");
    google_drive::remove(account_, in_drive(name));
    EXPECT_FALSE(on_drive(name));
    const auto left = named(name);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_TRUE(left.at(0).trashed) << "The file was deleted for good or is still in its folder.";
    EXPECT_THROW(google_drive::remove(account_, in_drive(name)), http::Error);
}

TEST_F(GoogleDriveLiveTest, MoveRenamesMovesAndNeverReplaces)
{
    put_on_drive("a/alt.txt", "alt");
    put_on_drive("b/besetzt.txt", "besetzt");
    google_drive::move(account_, in_drive("a/alt.txt"), in_drive("a/neu.txt"));
    google_drive::move(account_, in_drive("a/neu.txt"), in_drive("b/neu.txt"));
    EXPECT_FALSE(on_drive("a/alt.txt"));
    EXPECT_FALSE(on_drive("a/neu.txt"));
    EXPECT_EQ(content_on_drive("b/neu.txt"), "alt");
    EXPECT_THROW(google_drive::move(account_, in_drive("b/neu.txt"), in_drive("b/besetzt.txt")), http::Error);
    EXPECT_EQ(content_on_drive("b/besetzt.txt"), "besetzt");
}

TEST_F(GoogleDriveLiveTest, MakeDirectoryAcceptsAnExistingFolderButNotAFile)
{
    google_drive::make_directory(account_, in_drive("neu"));
    EXPECT_NO_THROW(google_drive::make_directory(account_, in_drive("neu")));
    EXPECT_EQ(google_drive::list(account_, in_drive()).size(), 1u) << "The second call made a second folder of the same name.";
    put_on_drive("datei", "x");
    EXPECT_THROW(google_drive::make_directory(account_, in_drive("datei")), http::Error);
}

TEST_F(GoogleDriveLiveTest, RemoteFunctionsReachTheDriveThroughTheAccountName)
{
    put_on_drive("ordner/brief.txt", "hallo");
    EXPECT_TRUE(ssh_exists(remote_path("ordner/brief.txt")));
    EXPECT_TRUE(ssh_is_directory(remote_path("ordner")));
    EXPECT_TRUE(ssh_mkdir_p(remote_path("a/b/c")));
    EXPECT_TRUE(ssh_rename(remote_path("ordner/brief.txt"), remote_path("ordner/post.txt")));
    EXPECT_EQ(ssh_list_dir(remote_path("ordner")).size(), 1u);
    EXPECT_TRUE(ssh_remove_file(remote_path("ordner/post.txt")));
    EXPECT_FALSE(ssh_exists(remote_path("ordner/post.txt")));
}

TEST_F(GoogleDriveLiveTest, SingleFileIsCopiedIntoAFolderOfTheDrive)  // [GoogleDriveLiveTests.md#single-file]
{
    write(source_ / "test.txt", "eine datei");
    const SyncItem item(L"Datei", SyncItemType::File, (source_ / "test.txt").wstring(), remote_path(), L"*.*");
    EXPECT_EQ(copy_into_folder(item), 1u);
    EXPECT_EQ(content_on_drive("test.txt"), "eine datei");
    EXPECT_EQ(copy_into_folder(item), 0u) << "An unchanged file was uploaded again.";
}

TEST_F(GoogleDriveLiveTest, TreeIsUploadedOnceAndThenLeftAlone)
{
    write(source_ / "oben.txt", "1");
    write(source_ / "tief" / "tiefer" / "unten.txt", "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 2u);
    EXPECT_EQ(content_on_drive("sicherung/oben.txt"), "1");
    EXPECT_EQ(content_on_drive("sicherung/tief/tiefer/unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(upload_item()), 0u) << "An unchanged tree was uploaded again.";
}

TEST_F(GoogleDriveLiveTest, TreeIsDownloaded)
{
    put_on_drive("sicherung/oben.txt", "1");
    put_on_drive("sicherung/tief/unten.txt", "22");
    const fs::path target = local_ / "ziel";
    fs::create_directories(target);
    const SyncItem item(L"Herunter", SyncItemType::Tree, remote_path("sicherung"), target.wstring(), L"*");
    EXPECT_EQ(tu::run_sync(item), 2u);
    EXPECT_EQ(content_of(target / "oben.txt"), "1");
    EXPECT_EQ(content_of(target / "tief" / "unten.txt"), "22");
    EXPECT_EQ(tu::run_sync(item), 0u) << "An unchanged tree was downloaded again.";
}

TEST_F(GoogleDriveLiveTest, ChangedFileDeletionAndEarlierVersionReachTheDrive)
{
    write(source_ / "bericht.txt", "alt");
    write(source_ / "geht.txt", "2");
    fs::last_write_time(source_ / "bericht.txt", fs::file_time_type::clock::now() - std::chrono::hours(10));
    SyncItem item = upload_item();
    item.delete_in_target(true);
    item.father_son_principle(true);
    ASSERT_EQ(tu::run_sync(item), 2u);
    write(source_ / "bericht.txt", "neuer");
    fs::remove(source_ / "geht.txt");
    tu::run_sync(item);
    EXPECT_EQ(content_on_drive("sicherung/bericht.txt"), "neuer");
    EXPECT_EQ(content_on_drive("sicherung/bericht.txt.bak.1"), "alt");
    EXPECT_FALSE(on_drive("sicherung/geht.txt"));
}

TEST_F(GoogleDriveLiveTest, TwoWaySyncCarriesChangesAndDeletionsBothWays)
{
    write(source_ / "lokal.txt", "von hier");
    put_on_drive("sicherung/fern.txt", "von dort");
    SyncItem item = upload_item();
    item.two_way_sync(true);
    SyncState state(item.source(), item.target());
    tu::run_sync(item, state);
    EXPECT_EQ(content_on_drive("sicherung/lokal.txt"), "von hier");
    EXPECT_EQ(content_of(source_ / "fern.txt"), "von dort");
    EXPECT_EQ(tu::run_sync(item, state), 0u) << "A settled two-way pair was copied again.";

    const tu::ScopedDeleteMode permanent(DeleteMode::Permanent);
    fs::remove(source_ / "fern.txt");
    google_drive::remove(account_, in_drive("sicherung/lokal.txt"));
    tu::run_sync(item, state);
    EXPECT_FALSE(on_drive("sicherung/fern.txt")) << "The local deletion did not reach the drive.";
    EXPECT_FALSE(fs::exists(source_ / "lokal.txt")) << "The deletion in the drive did not come back.";
}

}
