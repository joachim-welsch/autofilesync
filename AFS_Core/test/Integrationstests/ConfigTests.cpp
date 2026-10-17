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
#include "Config.h"
#include "Logging.h"
#include "ScopedLoggingSetup.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <optional>
#include <StrUtil.h>
#include <RecycleBin.h>
#include <boost/property_tree/json_parser.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <Util.h>
#include "TestPaths.h"

using namespace AFS_Core;

using namespace util;
namespace fs = std::filesystem;

namespace {

const char* const REDIRECTED_VARIABLES[] = {  // [ConfigTests.md#REDIRECTED_VARIABLES]
#ifdef _WIN32
    "APPDATA", "USERPROFILE"
#else
    "HOME", "XDG_CONFIG_HOME"
#endif
};

std::optional<std::string> get_env_var(const char* name)
{
    const char* value = getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
}

void set_env_var(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unset_env_var(const char* name)  // an empty value is how the CRT deletes a variable; POSIX has its own call
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class ConfigPathTest : public ::testing::Test {  // [ConfigTests.md#ConfigPathTest]
protected:
    fs::path test_home_;
    std::vector<std::pair<const char*, std::optional<std::string>>> saved_variables_;

    fs::path expected_config_path() const  // where get_root_pathname() puts the file once the environment is redirected
    {
#ifdef _WIN32
        return test_home_ / "AppData" / "autofilesync" / "autofilesync.json";
#elif defined(_MACOS)
        return test_home_ / "Library" / "Application Support" / "autofilesync" / "autofilesync.json";
#else
        return test_home_ / ".config" / "autofilesync" / "autofilesync.json";
#endif
    }

    void SetUp() override
    {
        for (const char* name : REDIRECTED_VARIABLES)
            saved_variables_.push_back({name, get_env_var(name)});
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_home_ = fs::temp_directory_path() / ("autofilesync_configtest_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(test_home_);
        fs::create_directories(test_home_);
#ifdef _WIN32
        set_env_var("APPDATA", (test_home_ / "AppData").string());
        set_env_var("USERPROFILE", test_home_.string());
#else
        set_env_var("HOME", test_home_.string());
        unset_env_var("XDG_CONFIG_HOME");
#endif
    }

    void TearDown() override
    {
        for (const auto& [name, value] : saved_variables_) {
            if (value)
                set_env_var(name, *value);
            else
                unset_env_var(name);
        }
        fs::remove_all(test_home_);
    }
};

void write_file(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

std::string read_file(const fs::path& path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_legacy_plaintext_config(const fs::path& path, const std::wstring& name,  // [ConfigTests.md#write_legacy_plaintext_config]
    const std::wstring& host, const std::wstring& user, const std::wstring& password)
{
    boost::property_tree::wptree account_node, accounts_node, root;
    account_node.put(L"name", name);
    account_node.put(L"host", host);
    account_node.put(L"user", user);
    account_node.put(L"password", password);
    accounts_node.push_back({ L"SshAccount", account_node });
    root.push_back({ L"SshAccounts", accounts_node });
    fs::create_directories(path.parent_path());
    std::wofstream out(path);
    boost::property_tree::write_json(out, root);
}

} // namespace

#if !defined(_MACOS) && !defined(_WIN32)  // [ConfigTests.md#xdg-linux-only]
TEST_F(ConfigPathTest, HonorsXdgConfigHome)
{
    const fs::path xdg_home = test_home_ / "xdg";
    set_env_var("XDG_CONFIG_HOME", xdg_home.string());

    const std::wstring path = Config::get_root_pathname();

    EXPECT_EQ(fs::path(wstring_to_string(path)), xdg_home / "autofilesync" / "autofilesync.json");
}

TEST_F(ConfigPathTest, FallsBackToDotConfigWhenXdgUnset)
{
    const std::wstring path = Config::get_root_pathname();

    EXPECT_EQ(fs::path(wstring_to_string(path)), test_home_ / ".config" / "autofilesync" / "autofilesync.json");
}
#endif

TEST_F(ConfigPathTest, MigratesLegacyConfigFile)
{
    const fs::path legacy_path = test_home_ / "autofilesync.json";
    write_file(legacy_path, "legacy content");

    const std::wstring path = Config::get_root_pathname();

    EXPECT_TRUE(fs::exists(wstring_to_string(path)));
    EXPECT_EQ(read_file(wstring_to_string(path)), "legacy content");
    EXPECT_FALSE(fs::exists(legacy_path));
}

TEST_F(ConfigPathTest, DoesNotOverwriteExistingConfigDuringMigration)
{
    const fs::path legacy_path = test_home_ / "autofilesync.json";
    const fs::path new_path = expected_config_path();
    write_file(legacy_path, "legacy content");
    write_file(new_path, "current content");

    Config::get_root_pathname();

    EXPECT_EQ(read_file(new_path), "current content");
    EXPECT_TRUE(fs::exists(legacy_path));
}

TEST_F(ConfigPathTest, FileChangedOnDiskFollowsReadAndWrite)  // [ConfigTests.md#FileChangedOnDiskFollowsReadAndWrite]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().pathname(target_path.wstring());
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    EXPECT_FALSE(Config::instance().file_changed_on_disk()) << "The own write counts as a foreign change.";

    fs::last_write_time(target_path, fs::last_write_time(target_path) + std::chrono::seconds(2));
    EXPECT_TRUE(Config::instance().file_changed_on_disk()) << "A newer file was not noticed.";

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));
    EXPECT_FALSE(Config::instance().file_changed_on_disk()) << "The file still counts as changed after re-reading it.";
}

TEST_F(ConfigPathTest, FileChangedOnDiskSeesAFileThatAppears)  // [ConfigTests.md#FileChangedOnDiskFollowsReadAndWrite]
{
    const fs::path target_path = test_home_ / "appears.json";
    Config::instance().pathname(target_path.wstring());
    EXPECT_FALSE(Config::instance().read(target_path.wstring()));
    EXPECT_FALSE(Config::instance().file_changed_on_disk()) << "A file that is still missing counts as changed.";

    write_file(target_path, "{}");

    EXPECT_TRUE(Config::instance().file_changed_on_disk()) << "The file's appearance was not noticed.";
}

TEST_F(ConfigPathTest, WriteCreatesMissingDirectories)
{
    const fs::path target_path = test_home_ / "does" / "not" / "exist" / "autofilesync.json";

    const bool result = Config::instance().write(target_path.wstring());

    EXPECT_TRUE(result);
    EXPECT_TRUE(fs::exists(target_path));
}

TEST_F(ConfigPathTest, SshAccountPasswordIsNotStoredAsPlaintext)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    const std::wstring secret = L"testonly-not-a-real-secret-42";
    Config::instance().reset();
    SshAccount account;
    account.set_name("pi");
    account.set_host("raspberrypi.local");
    account.set_user("pi");
    account.set_password(util::wstring_to_string(secret));
    Config::instance().add(account);

    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    const std::string file_content = read_file(target_path);
    EXPECT_EQ(file_content.find(util::wstring_to_string(secret)), std::string::npos);
    EXPECT_NE(file_content.find("AES256GCM"), std::string::npos);
}

TEST_F(ConfigPathTest, SshAccountPasswordRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    const std::wstring secret = L"testonly-not-a-real-secret-42";
    Config::instance().reset();
    SshAccount account;
    account.set_name("pi");
    account.set_host("raspberrypi.local");
    account.set_user("pi");
    account.set_password(util::wstring_to_string(secret));
    Config::instance().add(account);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto accounts = Config::instance().ssh_accounts_snapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), util::wstring_to_string(secret));
}

TEST_F(ConfigPathTest, SshAccountPasswordWithSpecialCharactersRoundTrips)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    const std::string secret = "a!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ äöüÄÖÜß€";  // [ConfigTests.md#utf8-password-roundtrip]
    Config::instance().reset();
    SshAccount account;
    account.set_name("pi");
    account.set_host("raspberrypi.local");
    account.set_user("pi");
    account.set_password(secret);
    Config::instance().add(account);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto accounts = Config::instance().ssh_accounts_snapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), secret);
}

TEST_F(ConfigPathTest, LegacyPlaintextPasswordStillLoads)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    write_legacy_plaintext_config(target_path, L"pi", L"raspberrypi.local", L"pi", L"legacy-plaintext-secret");

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto accounts = Config::instance().ssh_accounts_snapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), "legacy-plaintext-secret");
}

TEST_F(ConfigPathTest, LegacyPlaintextPasswordIsEncryptedOnNextSave)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    write_legacy_plaintext_config(target_path, L"pi", L"raspberrypi.local", L"pi", L"legacy-plaintext-secret");
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    const std::string file_content = read_file(target_path);
    EXPECT_EQ(file_content.find("legacy-plaintext-secret"), std::string::npos);
    EXPECT_NE(file_content.find("AES256GCM"), std::string::npos);
}

TEST_F(ConfigPathTest, ConcurrentSshAccountAccessDoesNotCrash)  // [ConfigTests.md#ConcurrentSshAccountAccessDoesNotCrash]
{
    Config::instance().reset();
    for (int i = 0; i < 5; ++i) {
        SshAccount account;
        account.set_name("acc" + std::to_string(i));
        account.set_host("host");
        account.set_user("user");
        account.set_password("pw");
        Config::instance().add(account);
    }

    std::atomic<bool> stop{false};
    std::atomic<bool> saw_inconsistency{false};

    std::thread writer([&] {
        int i = 5;
        while (!stop.load()) {
            SshAccount account;
            account.set_name("acc" + std::to_string(i++));
            Config::instance().add(account);
            Config::instance().remove_ssh_account(0);
        }
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto snapshot = Config::instance().ssh_accounts_snapshot();
                for (const auto& acc : snapshot) {
                    if (acc.name().empty()) // a torn/half-constructed element would show up here
                        saw_inconsistency = true;
                }
                Config::instance().find_ssh_account_by_name("acc0");
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();

    EXPECT_FALSE(saw_inconsistency.load());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConcurrentSyncItemAccessDoesNotCrash)  // [ConfigTests.md#ConcurrentSyncItemAccessDoesNotCrash]
{
    Config::instance().reset();
    for (int i = 0; i < 5; ++i)
        Config::instance().add(SyncItem(L"item" + std::to_wstring(i), SyncItemType::File, L"src", L"dst", L""));

    std::atomic<bool> stop{false};
    std::atomic<bool> saw_inconsistency{false};

    std::thread writer([&] {
        int i = 5;
        while (!stop.load()) {
            Config::instance().add(SyncItem(L"item" + std::to_wstring(i++), SyncItemType::File, L"src", L"dst", L""));
            Config::instance().remove(0);
        }
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto snapshot = Config::instance().syncitems();
                for (const auto& si : snapshot) {
                    if (si.name().empty()) // a torn/half-constructed element would show up here
                        saw_inconsistency = true;
                }
                Config::instance().find_by_name(L"item0");
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();

    EXPECT_FALSE(saw_inconsistency.load());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, RefreshLastSyncUpdatesTheLiveItem)  // [ConfigTests.md#RefreshLastSyncUpdatesTheLiveItem]
{
    Config::instance().reset();
    Config::instance().add(SyncItem(L"item", SyncItemType::File, L"src", L"dst", L""));
    auto before = Config::instance().find_by_name(L"item");
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->last_sync(), std::chrono::steady_clock::time_point::min());

    const auto now = std::chrono::steady_clock::now();
    Config::instance().refresh_last_sync(L"item");

    auto after = Config::instance().find_by_name(L"item");
    ASSERT_TRUE(after.has_value());
    EXPECT_GE(after->last_sync(), now);

    Config::instance().reset();
}

TEST_F(ConfigPathTest, WriteFailureDoesNotCorruptExistingConfig)  // [ConfigTests.md#WriteFailureDoesNotCorruptExistingConfig]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().add(SyncItem(L"original", SyncItemType::File, L"src", L"dst", L""));
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    const std::string original_content = read_file(target_path);
    ASSERT_FALSE(original_content.empty());

    const fs::path tmp_path = target_path.wstring() + L".tmp";  // [ConfigTests.md#occupied-temp-path]
    fs::create_directory(tmp_path);

    Config::instance().add(SyncItem(L"should-not-be-saved", SyncItemType::File, L"src2", L"dst2", L""));
    EXPECT_FALSE(Config::instance().write(target_path.wstring()));
    EXPECT_EQ(read_file(target_path), original_content);

    fs::remove_all(tmp_path);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, FindByNameReturnsCopyAndUpdateByNameWritesBack)  // [ConfigTests.md#FindByNameReturnsCopyAndUpdateByNameWritesBack]
{
    Config::instance().reset();
    Config::instance().add(SyncItem(L"item", SyncItemType::File, L"src", L"dst", L""));

    auto copy = Config::instance().find_by_name(L"item");
    ASSERT_TRUE(copy.has_value());
    copy->source(L"changed_src");
    EXPECT_EQ(Config::instance().find_by_name(L"item")->source(), L"src");  // the copy must not touch Config's live item
    EXPECT_TRUE(Config::instance().update_by_name(L"item", *copy));  // only the explicit locked write-back does
    EXPECT_EQ(Config::instance().find_by_name(L"item")->source(), L"changed_src");

    EXPECT_TRUE(Config::instance().erase_by_name(L"item"));
    EXPECT_FALSE(Config::instance().find_by_name(L"item").has_value());
    EXPECT_FALSE(Config::instance().erase_by_name(L"item"));
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConcurrentUpdateByNameAndSnapshotDoesNotCrash)
{
    Config::instance().reset();
    const std::wstring long_a(256, L'a');  // long strings force heap allocation, making a torn unlocked write visible
    const std::wstring long_b(256, L'b');
    Config::instance().add(SyncItem(L"item", SyncItemType::File, long_a, L"dst", L""));

    std::atomic_bool stop{false};
    std::thread writer([&] {
        bool flip = false;
        while (!stop.load()) {
            SyncItem updated(L"item", SyncItemType::File, flip ? long_a : long_b, L"dst", L"");
            Config::instance().update_by_name(L"item", updated);
            flip = !flip;
        }
    });
    std::atomic_bool saw_torn_read{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto snapshot = Config::instance().syncitems();
                for (const auto& si : snapshot) {
                    const auto s = si.source();
                    if (s != long_a && s != long_b)
                        saw_torn_read = true;
                }
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();
    EXPECT_FALSE(saw_torn_read.load());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MalformedSyncItemIsSkippedNotHalfLoaded)  // [ConfigTests.md#MalformedSyncItemIsSkippedNotHalfLoaded]
{
    const fs::path config_path = test_home_ / "broken_item.json";  // [ConfigTests.md#broken-first-entry]
    write_file(config_path, R"({
        "Syncitems": {
            "Syncitem": {
                "type": "Datei",
                "source": "/tmp/broken_src",
                "target": "/tmp/broken_dst"
            },
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "good",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].name(), L"good");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, RenamingSshAccountUpdatesReferencingSyncItems)  // [ConfigTests.md#RenamingSshAccountUpdatesReferencingSyncItems]
{
    Config::instance().reset();
    SshAccount acc;
    acc.set_name("oldname");
    acc.set_host("host1");
    acc.set_user("user1");
    acc.set_password("pw1");
    Config::instance().add(acc, true);

    SyncItem si(L"item", SyncItemType::File, L"oldname:/src", L"/local/dst", L"");
    ASSERT_TRUE(si.ssh_source());
    Config::instance().add(si, true);  // not referencing the renamed account: must be left untouched
    SyncItem unrelated(L"other", SyncItemType::File, L"/local/src2", L"/local/dst2", L"");
    Config::instance().add(unrelated, true);

    SshAccount renamed = acc;
    renamed.set_name("newname");
    renamed.set_host("host2");
    EXPECT_TRUE(Config::instance().update_ssh_account_references("oldname", renamed));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].source(), L"newname:/src"); // account name in the path prefix updated
    EXPECT_FALSE(Config::instance().ssh_account_referenced_by_any_item("oldname"));
    EXPECT_TRUE(Config::instance().ssh_account_referenced_by_any_item("newname"));
    EXPECT_EQ(items[1].source(), L"/local/src2"); // unrelated item unchanged

    EXPECT_FALSE(Config::instance().update_ssh_account_references("nonexistent", renamed));  // [ConfigTests.md#rename-without-references]
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MalformedSshAccountIsSkippedNotAllAccountsLost)  // [ConfigTests.md#MalformedSshAccountIsSkippedNotAllAccountsLost]
{
    const fs::path config_path = test_home_ / "broken_account.json";  // first account lacks "password" (and more); second is complete
    write_file(config_path, R"({
        "SshAccounts": {
            "SshAccount": {
                "name": "broken",
                "host": "brokenhost"
            },
            "SshAccount": {
                "name": "good",
                "host": "goodhost",
                "user": "gooduser",
                "password": "goodpw"
            }
        }
    })");

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    const auto accounts = Config::instance().ssh_accounts_snapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].name(), "good");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MalformedSettingsDoesNotBlockSyncitemsAndAccounts)
{
    const fs::path config_path = test_home_ / "broken_settings.json";  // [ConfigTests.md#missing-settings-fields]
    write_file(config_path, R"({
        "Settings": {
            "StartMinimzed": "true"
        },
        "SshAccounts": {
            "SshAccount": {
                "name": "acc",
                "host": "h",
                "user": "u",
                "password": "p"
            }
        },
        "Syncitems": {
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    EXPECT_EQ(Config::instance().ssh_accounts_snapshot().size(), 1u);
    EXPECT_EQ(Config::instance().syncitems().size(), 1u);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, FailedPasswordDecryptionDoesNotUseRawCiphertextAsPassword)  // [ConfigTests.md#FailedPasswordDecryptionDoesNotUseRawCiphertextAsPassword]
{
    const fs::path config_path = test_home_ / "bad_password.json";  // [ConfigTests.md#undecryptable-password]
    write_file(config_path, R"({
        "SshAccounts": {
            "SshAccount": {
                "name": "acc",
                "host": "h",
                "user": "u",
                "password": "$AES256GCM$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            }
        }
    })");

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));
    const auto accounts = Config::instance().ssh_accounts_snapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_TRUE(accounts[0].password().empty());
    EXPECT_EQ(accounts[0].password().find("AES256GCM"), std::string::npos);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, UmlautsRoundTripThroughSaveAndLoad)  // [ConfigTests.md#UmlautsRoundTripThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Exclusions exclusions;
    exclusions.push_back(L"Ärger*.tmp");
    Config::instance().add(SyncItem(L"Übersicht", SyncItemType::Tree, L"/home/jürgen/Größe", L"/mnt/Sicherung/Straße", L"",
        exclusions, SyncFlags(false, false, false, false, true), CompareItems()));
    Config::instance().settings().backup_proof_path(L"/home/jürgen/Nachweise");
    ASSERT_TRUE(Config::instance().write(target_path.wstring())) << "Eine Konfiguration mit Umlauten ließ sich nicht speichern.";

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].name(), L"Übersicht");
    EXPECT_EQ(items[0].source(), L"/home/jürgen/Größe");
    EXPECT_EQ(items[0].target(), L"/mnt/Sicherung/Straße");
    EXPECT_EQ(items[0].exclusions().at(0), L"Ärger*.tmp");
    EXPECT_EQ(Config::instance().settings().backup_proof_path(), L"/home/jürgen/Nachweise");
    std::ifstream in(target_path);
    const std::string bytes((std::istreambuf_iterator<char>(in)), {});
    EXPECT_NE(bytes.find("\xC3\x9C" "bersicht"), std::string::npos) << "Der Name steht nicht als UTF-8 in der Datei.";
    Config::instance().reset();
}

TEST_F(ConfigPathTest, NonLatinNamesStayReadableInTheFile)  // [ConfigTests.md#NonLatinNamesStayReadableInTheFile]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    const std::wstring name = L"Звіт \U0001F600";
    Config::instance().reset();
    Config::instance().add(SyncItem(name, SyncItemType::Dir, L"/home/олена/Документи", L"/mnt/Резерв", L""));
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    std::ifstream in(target_path);
    const std::string bytes((std::istreambuf_iterator<char>(in)), {});
    EXPECT_NE(bytes.find(util::wstring_to_string(name)), std::string::npos) << "Der Name steht nicht lesbar in der Datei:\n" << bytes;
    EXPECT_EQ(bytes.find("\\u"), std::string::npos) << "Die Datei enthält maskierte Zeichen:\n" << bytes;
    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));
    ASSERT_EQ(Config::instance().syncitems().size(), 1u);
    EXPECT_EQ(Config::instance().syncitems()[0].name(), name);
    EXPECT_EQ(Config::instance().syncitems()[0].source(), L"/home/олена/Документи");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, HandWrittenUtf8ConfigIsRead)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().backup_proof_path(L"/home/PLATZHALTER");
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    std::string bytes;
    {
        std::ifstream in(target_path);
        bytes.assign(std::istreambuf_iterator<char>(in), {});
    }
    bytes.replace(bytes.find("PLATZHALTER"), 11, "j\xC3\xBCrgen");  // as an editor saving UTF-8 leaves it
    std::ofstream(target_path) << bytes;
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_EQ(Config::instance().settings().backup_proof_path(), L"/home/jürgen");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ExclusionsRoundTripThroughSaveAndLoad)  // [ConfigTests.md#ExclusionsRoundTripThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    exclusions.push_back(L"node_modules");
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", exclusions,
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    const Exclusions loaded = items[0].exclusions();
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0], L"*.tmp");
    EXPECT_EQ(loaded[1], L"node_modules");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, BackupProofRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.backup_proof());
    si.backup_proof(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].backup_proof());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, BackupProofDefaultsToFalseForOlderConfigFiles)  // [ConfigTests.md#BackupProofDefaultsToFalseForOlderConfigFiles]
{
    const fs::path config_path = test_home_ / "autofilesync.json";
    write_file(config_path, R"({
        "Syncitems": {
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].backup_proof());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, DeleteInTargetRoundTripsThroughSaveAndLoad)  // [ConfigTests.md#DeleteInTargetRoundTripsThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.delete_in_target());
    si.delete_in_target(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].delete_in_target());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MoveCopyWhenTargetNewerAndFatherSonRoundTripThroughSaveAndLoad)  // [ConfigTests.md#MoveCopyWhenTargetNewerAndFatherSonRoundTripThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(true, false, true, true, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].del_aftercopy());
    EXPECT_TRUE(items[0].copy_when_target_newer());
    EXPECT_TRUE(items[0].father_son_principle());
    EXPECT_FALSE(items[0].two_way_sync());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, DeleteInTargetStaysOffThroughSaveAndLoad)  // [ConfigTests.md#DeleteInTargetStaysOffThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].delete_in_target());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, DeleteInTargetDefaultsToFalseForOlderConfigFiles)  // [ConfigTests.md#DeleteInTargetDefaultsToFalseForOlderConfigFiles]
{
    const fs::path config_path = test_home_ / "legacy_syncdeletions.json";
    write_file(config_path, R"({
        "Syncitems": {
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].delete_in_target());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SyncEmptyFilesRoundTripsThroughSaveAndLoad)  // [ConfigTests.md#SyncEmptyFilesRoundTripsThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.sync_empty_files());
    si.sync_empty_files(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].sync_empty_files());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SyncEmptyFilesStaysOffThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].sync_empty_files());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, WarnIfUnreachableIsOnByDefaultAndRoundTripsOff)  // [ConfigTests.md#WarnIfUnreachableIsOnByDefaultAndRoundTripsOff]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_TRUE(si.warn_if_unreachable());
    si.warn_if_unreachable(false);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].warn_if_unreachable());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, TimeToleranceRoundTripsThroughSaveAndLoad)  // [ConfigTests.md#TimeToleranceRoundTripsThroughSaveAndLoad]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    CompareItems ci;
    ci.time(true);
    ci.time_tolerance_seconds(7);
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(), SyncFlags(false, false, false, false, true), ci);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].compare_items().time_tolerance_seconds(), 7);
    EXPECT_TRUE(items[0].compare_items().time());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, TimeToleranceDefaultsToTwoSecondsForOlderConfigFiles)  // [ConfigTests.md#TimeToleranceDefaultsToTwoSecondsForOlderConfigFiles]
{
    const fs::path config_path = test_home_ / "legacy_timetolerance.json";
    write_file(config_path, R"({
        "Syncitems": {
            "Syncitem": {
                "compareitems": { "size": "false", "time": "true", "content": "false" },
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].compare_items().time_tolerance_seconds(), DEFAULT_TIME_TOLERANCE_SECONDS);
    EXPECT_TRUE(items[0].compare_items().time());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SyncEmptyFilesDefaultsToFalseForOlderConfigFiles)  // [ConfigTests.md#SyncEmptyFilesDefaultsToFalseForOlderConfigFiles]
{
    const fs::path config_path = test_home_ / "legacy_syncemptyfiles.json";
    write_file(config_path, R"({
        "Syncitems": {
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].sync_empty_files());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, BackupProofPathRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().backup_proof_path(L"/tmp/nachweise");
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().settings().backup_proof_path().empty());
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_EQ(Config::instance().settings().backup_proof_path(), L"/tmp/nachweise");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, LanguageRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().language("uk");
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().settings().language().empty());
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_EQ(Config::instance().settings().language(), "uk");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, RecycleBinPathRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    const fs::path bin_path = test_home_ / "papierkorb";
    Config::instance().reset();
    RecycleBin::instance().bin_path(bin_path);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    RecycleBin::instance().bin_path("");
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_EQ(RecycleBin::instance().bin_path(), bin_path);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, DeleteModeRoundTripsThroughSaveAndLoad)
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().delete_mode(DeleteMode::Permanent);
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    Config::instance().reset();
    ASSERT_EQ(Config::instance().settings().delete_mode(), DeleteMode::RecycleBin);
    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_EQ(Config::instance().settings().delete_mode(), DeleteMode::Permanent);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConfigWithoutDeleteModeFallsBackToRecycleBin)  // [ConfigTests.md#ConfigWithoutDeleteModeFallsBackToRecycleBin]
{
    const fs::path config_path = test_home_ / "legacy_deletemode.json";
    write_file(config_path, R"({
        "Settings": {
            "StartMinimzed": "false",
            "MinimizeInsteadOfClose": "false",
            "Autostart": "false",
            "StartSyncAtStart": "false",
            "ShowErrors": "true",
            "ShowWarnings": "false",
            "ShowInfos": "false"
        }
    })");
    Config::instance().settings().delete_mode(DeleteMode::Permanent);

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    EXPECT_EQ(Config::instance().settings().delete_mode(), DeleteMode::RecycleBin);
    EXPECT_TRUE(Config::instance().settings().show_errors());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConfigWithoutRecycleBinPathKeepsCurrentPath)  // [ConfigTests.md#ConfigWithoutRecycleBinPathKeepsCurrentPath]
{
    const fs::path config_path = test_home_ / "legacy_settings.json";
    write_file(config_path, R"({
        "Settings": {
            "StartMinimzed": "true",
            "MinimizeInsteadOfClose": "false",
            "Autostart": "false",
            "StartSyncAtStart": "false",
            "ShowErrors": "true",
            "ShowWarnings": "false",
            "ShowInfos": "false"
        }
    })");
    const fs::path bin_path = test_home_ / "papierkorb";
    RecycleBin::instance().bin_path(bin_path);

    ASSERT_TRUE(Config::instance().read(config_path.wstring()));

    EXPECT_EQ(RecycleBin::instance().bin_path(), bin_path);
    EXPECT_TRUE(Config::instance().settings().start_minimized());
    EXPECT_TRUE(Config::instance().settings().show_errors());
    Config::instance().reset();
}

namespace {

SmtpAccount test_smtp_account(const std::wstring& password)
{
    SmtpAccount smtp;
    smtp.server = L"mail.example.org";
    smtp.port = 465;
    smtp.user = L"sicherung@example.org";
    smtp.password = password;
    smtp.from = L"sicherung@example.org";
    return smtp;
}

void configure_logging(const fs::path& log_file, const std::wstring& smtp_password)  // [ConfigTests.md#configure_logging]
{
    Logging& log = Logging::instance();
    log.set_log_level(LogLevel::Warning);
    log.set_log_file_path_name(log_file.wstring());
    log.retention_days(30);
    log.set_editor_path_name(L"/usr/bin/gedit");
    log.email().notify_active(true);
    log.email().notify_address(L"admin@example.org");
    log.email().smtp_account(test_smtp_account(smtp_password));
}

void disturb_logging(const fs::path& other_log_file)  // [ConfigTests.md#configure_logging]
{
    Logging& log = Logging::instance();
    log.set_log_level(LogLevel::Debug);
    log.set_log_file_path_name(other_log_file.wstring());
    log.retention_days(1);
    log.set_editor_path_name(L"/usr/bin/anderer-editor");
    log.email().notify_active(false);
    log.email().notify_address(L"niemand@example.org");
    log.email().smtp_account(SmtpAccount());
}

std::string hand_written_logging_section(const fs::path& log_file, const std::string& extra_keys)
{
    return R"({
        "Logging": {
            "LogLevel": "Info",
            "LogFilePathName": ")" + log_file.generic_string() + R"(",
            "RetentionDays": "7",
            "EditorPathName": "/usr/bin/nautilus")" + extra_keys + R"(
        }
    })";
}

} // namespace

TEST_F(ConfigPathTest, LoggingSetupRoundTripsThroughSaveAndLoad)  // [ConfigTests.md#LoggingSetupRoundTripsThroughSaveAndLoad]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path target_path = test_home_ / "autofilesync.json";
    const fs::path log_file = test_home_ / "logs" / "afs.log";
    Config::instance().reset();
    configure_logging(log_file, L"testonly-smtp-secret");
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    disturb_logging(test_home_ / "anders.log");

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    Logging& log = Logging::instance();
    EXPECT_EQ(log.get_log_level(), LogLevel::Warning);
    EXPECT_EQ(log.get_log_file_path_name(), log_file.wstring());
    EXPECT_EQ(log.retention_days(), 30);
    EXPECT_EQ(log.get_editor_path_name(), L"/usr/bin/gedit");
    EXPECT_TRUE(log.email().notify_active());
    EXPECT_EQ(log.email().notify_address(), L"admin@example.org");
    EXPECT_TRUE(log.email().smtp_account() == test_smtp_account(L"testonly-smtp-secret")) << "SMTP-Konto samt Passwort muss unveraendert zurueckkommen";
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SmtpPasswordIsNotStoredAsPlaintext)  // [ConfigTests.md#SmtpPasswordIsNotStoredAsPlaintext]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    configure_logging(test_home_ / "afs.log", L"testonly-smtp-secret");

    ASSERT_TRUE(Config::instance().write(target_path.wstring()));

    const std::string file_content = read_file(target_path);
    EXPECT_EQ(file_content.find("testonly-smtp-secret"), std::string::npos) << file_content;
    EXPECT_NE(file_content.find("\"SmtpPassword\": \"$AES256GCM$"), std::string::npos) << file_content;
    Config::instance().reset();
}

TEST_F(ConfigPathTest, EmptySmtpPasswordStaysEmptyThroughSaveAndLoad)  // [ConfigTests.md#EmptySmtpPasswordStaysEmptyThroughSaveAndLoad]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    configure_logging(test_home_ / "afs.log", L"");
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    EXPECT_NE(read_file(target_path).find("\"SmtpPassword\": \"\""), std::string::npos) << "ein leeres Passwort wird nicht verschluesselt";
    Logging::instance().email().smtp_account(test_smtp_account(L"nicht leer"));

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));

    EXPECT_TRUE(Logging::instance().email().smtp_account().password.empty());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, UndecryptableSmtpPasswordIsDroppedNotUsedAsIs)  // [ConfigTests.md#UndecryptableSmtpPasswordIsDroppedNotUsedAsIs]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path config_path = test_home_ / "bad_smtp_password.json";
    write_file(config_path, hand_written_logging_section(test_home_ / "afs.log", R"(,
            "SmtpServer": "mail.example.org",
            "SmtpFrom": "sicherung@example.org",
            "SmtpPassword": "$AES256GCM$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")"));

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));

    const SmtpAccount smtp = Logging::instance().email().smtp_account();
    EXPECT_EQ(smtp.server, L"mail.example.org") << "die uebrigen SMTP-Werte bleiben lesbar";
    EXPECT_TRUE(smtp.password.empty());
    EXPECT_EQ(smtp.password.find(L"AES256GCM"), std::wstring::npos);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, LoggingSectionWithoutNotificationKeysLoadsWithDefaults)  // [ConfigTests.md#LoggingSectionWithoutNotificationKeysLoadsWithDefaults]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path config_path = test_home_ / "old_logging.json";
    const fs::path log_file = test_home_ / "afs.log";
    configure_logging(test_home_ / "anders.log", L"geheim");
    write_file(config_path, hand_written_logging_section(log_file, ""));

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));

    Logging& log = Logging::instance();
    EXPECT_EQ(log.get_log_level(), LogLevel::Info);
    EXPECT_EQ(log.get_log_file_path_name(), log_file.generic_wstring());  // the JSON holds the generic form
    EXPECT_EQ(log.retention_days(), 7);
    EXPECT_FALSE(log.email().notify_active()) << "ohne Schluessel ist die Benachrichtigung aus";
    EXPECT_TRUE(log.email().notify_address().empty());
    EXPECT_TRUE(log.email().smtp_account() == SmtpAccount()) << "ohne Schluessel gilt das leere SMTP-Konto mit Port 587";
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConfigWithoutLoggingSectionKeepsTheCurrentLoggingSetup)  // [ConfigTests.md#ConfigWithoutLoggingSectionKeepsTheCurrentLoggingSetup]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path config_path = test_home_ / "no_logging.json";
    const fs::path log_file = test_home_ / "afs.log";
    configure_logging(log_file, L"geheim");
    write_file(config_path, R"({ "Settings": { "StartMinimized": "false" } })");

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));

    Logging& log = Logging::instance();
    EXPECT_EQ(log.get_log_level(), LogLevel::Warning);
    EXPECT_EQ(log.get_log_file_path_name(), log_file.wstring());
    EXPECT_EQ(log.retention_days(), 30);
    EXPECT_TRUE(log.email().notify_active());
    EXPECT_TRUE(log.email().smtp_account() == test_smtp_account(L"geheim"));
    Config::instance().reset();
}

TEST_F(ConfigPathTest, UnknownLogLevelFallsBackToError)  // [ConfigTests.md#UnknownLogLevelFallsBackToError]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path config_path = test_home_ / "odd_level.json";
    std::string json = hand_written_logging_section(test_home_ / "afs.log", "");
    json.replace(json.find("\"Info\""), 6, "\"Verbose\"");
    write_file(config_path, json);
    Logging::instance().set_log_level(LogLevel::Debug);

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));

    EXPECT_EQ(Logging::instance().get_log_level(), LogLevel::Error);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MalformedLoggingSectionDoesNotBlockSyncitems)  // [ConfigTests.md#MalformedLoggingSectionDoesNotBlockSyncitems]
{
    const tu::ScopedLoggingSetup restore;
    const fs::path config_path = test_home_ / "broken_logging.json";
    write_file(config_path, R"({
        "Logging": {
            "LogFilePathName": "/tmp/afs.log"
        },
        "Syncitems": {
            "Syncitem": {
                "syncflags": {
                    "delAftercopy": "false",
                    "twoWaySync": "false",
                    "copyWhenTargetNewer": "false",
                    "fatherSonPrinciple": "false",
                    "enabled": "true"
                },
                "name": "item",
                "type": "Datei",
                "source": "/tmp/src",
                "target": "/tmp/dst",
                "wildcard": "*",
                "SyncCycle": "5"
            }
        }
    })");
    const std::wstring log_file_before = Logging::instance().get_log_file_path_name();

    EXPECT_TRUE(Config::instance().read(config_path.wstring()));

    EXPECT_EQ(Config::instance().syncitems().size(), 1u);
    EXPECT_EQ(Logging::instance().get_log_file_path_name(), log_file_before) << "ein unvollstaendiger Logging-Abschnitt wird als Ganzes verworfen";
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SyncPossibleNeedsALoadedConfigurationWithItems)  // [ConfigTests.md#SyncPossibleNeedsALoadedConfigurationWithItems]
{
    const fs::path target_path = test_home_ / "autofilesync.json";
    Config::instance().reset();
    EXPECT_FALSE(Config::sync_possible()) << "ohne Items gibt es nichts zu synchronisieren";
    Config::instance().add(SyncItem(L"item", SyncItemType::Dir, L"/src", L"/dst", L"*"));
    ASSERT_TRUE(Config::instance().write(target_path.wstring()));
    Config::instance().reset();

    ASSERT_TRUE(Config::instance().read(target_path.wstring()));
    EXPECT_TRUE(Config::sync_possible());

    EXPECT_TRUE(Config::instance().erase_by_name(L"item"));
    EXPECT_FALSE(Config::sync_possible());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, UnloadDropsTheInstanceAndTheNextOneStartsEmpty)  // [ConfigTests.md#UnloadDropsTheInstanceAndTheNextOneStartsEmpty]
{
    Config::instance().add(SyncItem(L"item", SyncItemType::Dir, L"/src", L"/dst", L"*"));
    ASSERT_TRUE(Config::exists());

    Config::instance().unload();

    EXPECT_FALSE(Config::exists());
    EXPECT_TRUE(Config::instance().syncitems().empty()) << "die neue Instanz darf nichts von der alten erben";
    EXPECT_TRUE(Config::exists());
}
