#include "pch.h"
#include "Config.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <optional>
#include <strutil.h>
#include <RecycleBin.h>
#include <boost/property_tree/json_parser.hpp>
#include <thread>
#include <atomic>
#include <vector>
#include <util.h>
#ifdef _WIN32
    #include <process.h>   // _getpid
#else
    #include <unistd.h>    // getpid
#endif

using namespace AFS_Core;

using namespace util;
namespace fs = std::filesystem;

namespace {

// The variables Config::getRootPathname() derives the configuration path from -
// APPDATA plus USERPROFILE for the legacy location on Windows, HOME (and
// XDG_CONFIG_HOME on Linux) elsewhere.
const char* const redirectedVariables[] = {
#ifdef _WIN32
    "APPDATA", "USERPROFILE"
#else
    "HOME", "XDG_CONFIG_HOME"
#endif
};

std::optional<std::string> getEnvVar(const char* name)
{
    const char* value = getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
}

void setEnvVar(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

// An empty value is how the CRT deletes a variable; POSIX has its own call.
void unsetEnvVar(const char* name)
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

int processId()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

// Points the environment at a fresh temp directory so Config::getRootPathname()
// can be exercised without touching the real user configuration.
class ConfigPathTest : public ::testing::Test {
protected:
    fs::path testHome_;
    std::vector<std::pair<const char*, std::optional<std::string>>> savedVariables_;

    // Where getRootPathname() puts the file once the environment is redirected.
    fs::path expectedConfigPath() const
    {
#ifdef _WIN32
        return testHome_ / "AppData" / "autofilesync" / "autofilesync.json";
#elif defined(_MACOS)
        return testHome_ / "Library" / "Application Support" / "autofilesync" / "autofilesync.json";
#else
        return testHome_ / ".config" / "autofilesync" / "autofilesync.json";
#endif
    }

    void SetUp() override
    {
        for (const char* name : redirectedVariables)
            savedVariables_.push_back({name, getEnvVar(name)});
        const std::string testName = testing::UnitTest::GetInstance()->current_test_info()->name();
        testHome_ = fs::temp_directory_path() / ("autofilesync_configtest_" + testName + "_" + std::to_string(processId()));
        fs::remove_all(testHome_);
        fs::create_directories(testHome_);
#ifdef _WIN32
        setEnvVar("APPDATA", (testHome_ / "AppData").string());
        setEnvVar("USERPROFILE", testHome_.string());
#else
        setEnvVar("HOME", testHome_.string());
        unsetEnvVar("XDG_CONFIG_HOME");
#endif
    }

    void TearDown() override
    {
        for (const auto& [name, value] : savedVariables_) {
            if (value)
                setEnvVar(name, *value);
            else
                unsetEnvVar(name);
        }
        fs::remove_all(testHome_);
    }
};

void writeFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

std::string readFile(const fs::path& path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Writes a config file in the pre-encryption format (plaintext "password" field)
// to verify existing users' configs still load after upgrading.
void writeLegacyPlaintextConfig(const fs::path& path, const std::wstring& name,
    const std::wstring& host, const std::wstring& user, const std::wstring& password)
{
    boost::property_tree::wptree accountNode, accountsNode, root;
    accountNode.put(L"name", name);
    accountNode.put(L"host", host);
    accountNode.put(L"user", user);
    accountNode.put(L"password", password);
    accountsNode.push_back({ L"SshAccount", accountNode });
    root.push_back({ L"SshAccounts", accountsNode });
    fs::create_directories(path.parent_path());
    std::wofstream out(path);
    boost::property_tree::write_json(out, root);
}

} // namespace

// XDG_CONFIG_HOME is a Linux/freedesktop.org convention only: on macOS
// Config::getRootPathname() always resolves under Library/Application Support,
// on Windows under APPDATA, so these two apply to Linux alone.
#if !defined(_MACOS) && !defined(_WIN32)
TEST_F(ConfigPathTest, HonorsXdgConfigHome)
{
    const fs::path xdgHome = testHome_ / "xdg";
    setEnvVar("XDG_CONFIG_HOME", xdgHome.string());

    const std::wstring path = Config::getRootPathname();

    EXPECT_EQ(fs::path(wstring_to_string(path)), xdgHome / "autofilesync" / "autofilesync.json");
}

TEST_F(ConfigPathTest, FallsBackToDotConfigWhenXdgUnset)
{
    const std::wstring path = Config::getRootPathname();

    EXPECT_EQ(fs::path(wstring_to_string(path)), testHome_ / ".config" / "autofilesync" / "autofilesync.json");
}
#endif

TEST_F(ConfigPathTest, MigratesLegacyConfigFile)
{
    const fs::path legacyPath = testHome_ / "autofilesync.json";
    writeFile(legacyPath, "legacy content");

    const std::wstring path = Config::getRootPathname();

    EXPECT_TRUE(fs::exists(wstring_to_string(path)));
    EXPECT_EQ(readFile(wstring_to_string(path)), "legacy content");
    EXPECT_FALSE(fs::exists(legacyPath));
}

TEST_F(ConfigPathTest, DoesNotOverwriteExistingConfigDuringMigration)
{
    const fs::path legacyPath = testHome_ / "autofilesync.json";
    const fs::path newPath = expectedConfigPath();
    writeFile(legacyPath, "legacy content");
    writeFile(newPath, "current content");

    Config::getRootPathname();

    EXPECT_EQ(readFile(newPath), "current content");
    EXPECT_TRUE(fs::exists(legacyPath));
}

TEST_F(ConfigPathTest, WriteCreatesMissingDirectories)
{
    const fs::path targetPath = testHome_ / "does" / "not" / "exist" / "autofilesync.json";

    const bool result = Config::instance().write(targetPath.wstring());

    EXPECT_TRUE(result);
    EXPECT_TRUE(fs::exists(targetPath));
}

TEST_F(ConfigPathTest, SshAccountPasswordIsNotStoredAsPlaintext)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    const std::wstring secret = L"testonly-not-a-real-secret-42";
    Config::instance().reset();
    SshAccount account;
    account.setName("pi");
    account.setHost("raspberrypi.local");
    account.setUser("pi");
    account.setPassword(util::wstring_to_string(secret));
    Config::instance().add(account);

    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    const std::string fileContent = readFile(targetPath);
    EXPECT_EQ(fileContent.find(util::wstring_to_string(secret)), std::string::npos);
    EXPECT_NE(fileContent.find("AES256GCM"), std::string::npos);
}

TEST_F(ConfigPathTest, SshAccountPasswordRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    const std::wstring secret = L"testonly-not-a-real-secret-42";
    Config::instance().reset();
    SshAccount account;
    account.setName("pi");
    account.setHost("raspberrypi.local");
    account.setUser("pi");
    account.setPassword(util::wstring_to_string(secret));
    Config::instance().add(account);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto accounts = Config::instance().sshAccountsSnapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), util::wstring_to_string(secret));
}

TEST_F(ConfigPathTest, SshAccountPasswordWithSpecialCharactersRoundTrips)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    // Passwords are arbitrary UTF-8 byte sequences; every stage of the chain
    // (AES-GCM, base64, JSON with its wide-string node values) has to hand back
    // exactly the same bytes, including the ones a JSON writer must escape.
    const std::string secret = "a!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ äöüÄÖÜß€";
    Config::instance().reset();
    SshAccount account;
    account.setName("pi");
    account.setHost("raspberrypi.local");
    account.setUser("pi");
    account.setPassword(secret);
    Config::instance().add(account);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto accounts = Config::instance().sshAccountsSnapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), secret);
}

TEST_F(ConfigPathTest, LegacyPlaintextPasswordStillLoads)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    writeLegacyPlaintextConfig(targetPath, L"pi", L"raspberrypi.local", L"pi", L"legacy-plaintext-secret");

    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto accounts = Config::instance().sshAccountsSnapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].password(), "legacy-plaintext-secret");
}

TEST_F(ConfigPathTest, LegacyPlaintextPasswordIsEncryptedOnNextSave)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    writeLegacyPlaintextConfig(targetPath, L"pi", L"raspberrypi.local", L"pi", L"legacy-plaintext-secret");
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    const std::string fileContent = readFile(targetPath);
    EXPECT_EQ(fileContent.find("legacy-plaintext-secret"), std::string::npos);
    EXPECT_NE(fileContent.find("AES256GCM"), std::string::npos);
}

// Regression test for Fehler 9: sshAccounts() used to hand out a live reference
// to the internal vector, so a GUI-thread erase()/edit could reallocate or
// mutate elements while a sync-thread search was iterating - classic
// concurrent-modification UB. Config now only exposes locked, copying
// accessors; hammering add/remove/find from multiple threads at once must
// not crash and must always see a consistent (never partially-erased) state.
TEST_F(ConfigPathTest, ConcurrentSshAccountAccessDoesNotCrash)
{
    Config::instance().reset();
    for (int i = 0; i < 5; ++i) {
        SshAccount account;
        account.setName("acc" + std::to_string(i));
        account.setHost("host");
        account.setUser("user");
        account.setPassword("pw");
        Config::instance().add(account);
    }

    std::atomic<bool> stop{false};
    std::atomic<bool> sawInconsistency{false};

    std::thread writer([&] {
        int i = 5;
        while (!stop.load()) {
            SshAccount account;
            account.setName("acc" + std::to_string(i++));
            Config::instance().add(account);
            Config::instance().removeSshAccount(0);
        }
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto snapshot = Config::instance().sshAccountsSnapshot();
                for (const auto& acc : snapshot) {
                    if (acc.name().empty()) // a torn/half-constructed element would show up here
                        sawInconsistency = true;
                }
                Config::instance().findSshAccountByName("acc0");
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();

    EXPECT_FALSE(sawInconsistency.load());
    Config::instance().reset();
}

// syncitems_ is read by the sync worker thread (via the snapshot returned from
// syncitems()) while the GUI thread concurrently adds/removes items - the same
// hazard already covered above for sshAccounts_.
TEST_F(ConfigPathTest, ConcurrentSyncItemAccessDoesNotCrash)
{
    Config::instance().reset();
    for (int i = 0; i < 5; ++i)
        Config::instance().add(SyncItem(L"item" + std::to_wstring(i), SyncItemType::File, L"src", L"dst", L""));

    std::atomic<bool> stop{false};
    std::atomic<bool> sawInconsistency{false};

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
                        sawInconsistency = true;
                }
                Config::instance().findByName(L"item0");
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();

    EXPECT_FALSE(sawInconsistency.load());
    Config::instance().reset();
}

// refreshLastSync() is how Synchronize writes the result of syncing a snapshot
// copy back onto the live SyncItem in Config (see Fehler 2: the worker no
// longer holds a reference into syncitems_ directly). Without this write-back,
// lastSync() would stay at its initial value forever and per-item syncCycle
// throttling (pauseElapsed() in Synchronize.cpp) would silently break.
TEST_F(ConfigPathTest, RefreshLastSyncUpdatesTheLiveItem)
{
    Config::instance().reset();
    Config::instance().add(SyncItem(L"item", SyncItemType::File, L"src", L"dst", L""));
    auto before = Config::instance().findByName(L"item");
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->lastSync(), std::chrono::steady_clock::time_point::min());

    const auto now = std::chrono::steady_clock::now();
    Config::instance().refreshLastSync(L"item");

    auto after = Config::instance().findByName(L"item");
    ASSERT_TRUE(after.has_value());
    EXPECT_GE(after->lastSync(), now);

    Config::instance().reset();
}

// Fehler 5: write() used to open pathname directly with std::ios_base::out,
// which truncates it immediately - a write_json failure partway through left
// a half-written/corrupt file with the previous, working config gone. write()
// now writes to a "<pathname>.tmp" file and renames it over pathname, so a
// failure must leave the original file completely untouched.
TEST_F(ConfigPathTest, WriteFailureDoesNotCorruptExistingConfig)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().add(SyncItem(L"original", SyncItemType::File, L"src", L"dst", L""));
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));
    const std::string originalContent = readFile(targetPath);
    ASSERT_FALSE(originalContent.empty());

    // Make the temp file write() would create fail by pre-occupying its path
    // with a directory - opening it as an output file fails.
    const fs::path tmpPath = targetPath.wstring() + L".tmp";
    fs::create_directory(tmpPath);

    Config::instance().add(SyncItem(L"should-not-be-saved", SyncItemType::File, L"src2", L"dst2", L""));
    EXPECT_FALSE(Config::instance().write(targetPath.wstring()));
    EXPECT_EQ(readFile(targetPath), originalContent);

    fs::remove_all(tmpPath);
    Config::instance().reset();
}

// Fehler 7 (round 4): find()/findByName() used to hand out raw SyncItem*
// into the mutex-guarded vector; the GUI then wrote through that pointer
// without the lock (and it dangled after add/remove reallocated the vector)
// while the sync thread concurrently copied the vector. They now return
// copies, and writes go through updateByName() under the lock.
TEST_F(ConfigPathTest, FindByNameReturnsCopyAndUpdateByNameWritesBack)
{
    Config::instance().reset();
    Config::instance().add(SyncItem(L"item", SyncItemType::File, L"src", L"dst", L""));

    auto copy = Config::instance().findByName(L"item");
    ASSERT_TRUE(copy.has_value());
    copy->source(L"changed_src");
    // Mutating the copy must not touch Config's live item ...
    EXPECT_EQ(Config::instance().findByName(L"item")->source(), L"src");
    // ... only the explicit locked write-back does.
    EXPECT_TRUE(Config::instance().updateByName(L"item", *copy));
    EXPECT_EQ(Config::instance().findByName(L"item")->source(), L"changed_src");

    EXPECT_TRUE(Config::instance().eraseByName(L"item"));
    EXPECT_FALSE(Config::instance().findByName(L"item").has_value());
    EXPECT_FALSE(Config::instance().eraseByName(L"item"));
    Config::instance().reset();
}

TEST_F(ConfigPathTest, ConcurrentUpdateByNameAndSnapshotDoesNotCrash)
{
    Config::instance().reset();
    // Long strings force heap allocation, making a torn unlocked write visible.
    const std::wstring longA(256, L'a');
    const std::wstring longB(256, L'b');
    Config::instance().add(SyncItem(L"item", SyncItemType::File, longA, L"dst", L""));

    std::atomic_bool stop{false};
    std::thread writer([&] {
        bool flip = false;
        while (!stop.load()) {
            SyncItem updated(L"item", SyncItemType::File, flip ? longA : longB, L"dst", L"");
            Config::instance().updateByName(L"item", updated);
            flip = !flip;
        }
    });
    std::atomic_bool sawTornRead{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const auto snapshot = Config::instance().syncitems();
                for (const auto& si : snapshot) {
                    const auto s = si.source();
                    if (s != longA && s != longB)
                        sawTornRead = true;
                }
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true;
    writer.join();
    for (auto& t : readers)
        t.join();
    EXPECT_FALSE(sawTornRead.load());
    Config::instance().reset();
}

// Fehler 9 (round 4): interpretJson() ignored interpretSyncItem()'s return
// value and added the half-initialized SyncItem anyway. One malformed entry
// (e.g. a missing mandatory field) produced a ghost item - enabled by default,
// type Undefined, empty paths - that showed up in the GUI and ran in every
// sync pass. Malformed entries must be skipped while the valid rest loads.
TEST_F(ConfigPathTest, MalformedSyncItemIsSkippedNotHalfLoaded)
{
    const fs::path configPath = testHome_ / "broken_item.json";
    // First entry lacks "name" (and more), second is complete - the loop must
    // survive the broken one and still load the good one.
    writeFile(configPath, R"({
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

    EXPECT_TRUE(Config::instance().read(configPath.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].name(), L"good");
    Config::instance().reset();
}

// Fehler 5 (round 5): a SyncItem's embedded SSH account is a snapshot taken
// when its source/target path was set, not a live lookup by name (see
// SyncItem::source()/target() via getAccountFromPath()). Renaming or editing
// the account in SshAccountsDialog left existing items pointing at a name/
// credentials that no longer resolve in Config, so they silently failed every
// sync pass afterward with no indication why.
TEST_F(ConfigPathTest, RenamingSshAccountUpdatesReferencingSyncItems)
{
    Config::instance().reset();
    SshAccount acc;
    acc.setName("oldname");
    acc.setHost("host1");
    acc.setUser("user1");
    acc.setPassword("pw1");
    Config::instance().add(acc, true);

    SyncItem si(L"item", SyncItemType::File, L"oldname:/src", L"/local/dst", L"");
    ASSERT_TRUE(si.sshSource());
    Config::instance().add(si, true);
    // Not referencing the renamed account: must be left untouched.
    SyncItem unrelated(L"other", SyncItemType::File, L"/local/src2", L"/local/dst2", L"");
    Config::instance().add(unrelated, true);

    SshAccount renamed = acc;
    renamed.setName("newname");
    renamed.setHost("host2");
    EXPECT_TRUE(Config::instance().updateSshAccountReferences("oldname", renamed));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].source(), L"newname:/src"); // account name in the path prefix updated
    EXPECT_FALSE(Config::instance().sshAccountReferencedByAnyItem("oldname"));
    EXPECT_TRUE(Config::instance().sshAccountReferencedByAnyItem("newname"));
    EXPECT_EQ(items[1].source(), L"/local/src2"); // unrelated item unchanged

    // Renaming an account nothing references is a no-op, not an error.
    EXPECT_FALSE(Config::instance().updateSshAccountReferences("nonexistent", renamed));
    Config::instance().reset();
}

// Fehler 3 (round 6): a single malformed SSH account entry aborted the whole
// readSshAccounts() loop, and a single malformed Settings/Logging field
// aborted interpretJson() before SshAccounts/Syncitems were even read - one
// bad field anywhere near the top of the file could make every sync item and
// every other SSH account appear to vanish. Both sections are now resilient:
// per-account for SshAccounts (mirroring the existing per-item Syncitems
// handling), and non-fatal for Settings/Logging.
TEST_F(ConfigPathTest, MalformedSshAccountIsSkippedNotAllAccountsLost)
{
    const fs::path configPath = testHome_ / "broken_account.json";
    // First account lacks "password" (and more); second is complete.
    writeFile(configPath, R"({
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

    EXPECT_TRUE(Config::instance().read(configPath.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    const auto accounts = Config::instance().sshAccountsSnapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_EQ(accounts[0].name(), "good");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, MalformedSettingsDoesNotBlockSyncitemsAndAccounts)
{
    const fs::path configPath = testHome_ / "broken_settings.json";
    // "Settings" node present but missing every field it needs; "Logging"
    // absent entirely (also handled via get_child_optional, but exercises
    // the same non-fatal-failure path for Settings specifically here).
    writeFile(configPath, R"({
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

    EXPECT_TRUE(Config::instance().read(configPath.wstring()));
    EXPECT_TRUE(Config::instance().ready());
    EXPECT_EQ(Config::instance().sshAccountsSnapshot().size(), 1u);
    EXPECT_EQ(Config::instance().syncitems().size(), 1u);
    Config::instance().reset();
}

// Fehler 4 (round 7): readSshAccounts() used decrypted.value_or(storedPassword),
// conflating "genuine legacy plaintext" (decrypt() returns nullopt because
// there's no marker) with "encrypted but failed to decrypt" (decrypt() also
// returns nullopt, e.g. the encryption key was lost/changed) - the second
// case silently used the raw "$AES256GCM$..." ciphertext blob as the literal
// SSH password instead of surfacing that decryption failed.
TEST_F(ConfigPathTest, FailedPasswordDecryptionDoesNotUseRawCiphertextAsPassword)
{
    const fs::path configPath = testHome_ / "bad_password.json";
    // Well-formed marker + base64 blob (long enough to pass the corruption
    // length check) that can never authenticate against any key - decrypt()
    // must fail, and the account's password must not become this raw string.
    writeFile(configPath, R"({
        "SshAccounts": {
            "SshAccount": {
                "name": "acc",
                "host": "h",
                "user": "u",
                "password": "$AES256GCM$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            }
        }
    })");

    EXPECT_TRUE(Config::instance().read(configPath.wstring()));
    const auto accounts = Config::instance().sshAccountsSnapshot();
    ASSERT_EQ(accounts.size(), 1u);
    EXPECT_TRUE(accounts[0].password().empty());
    EXPECT_EQ(accounts[0].password().find("AES256GCM"), std::string::npos);
    Config::instance().reset();
}

// Exclusions feature: getExclusions() was a TODO stub that always returned
// empty, so exclusion patterns written to the config file were silently
// dropped on the next load - writeSyncItem() already wrote them correctly,
// but nothing read them back.
TEST_F(ConfigPathTest, ExclusionsRoundTripThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    exclusions.push_back(L"node_modules");
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", exclusions,
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

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
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.backupProof());
    si.backupProof(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].backupProof());
    Config::instance().reset();
}

// A config file written before the flag existed has no "backupProof" node;
// loading it must fall back to the default instead of throwing.
TEST_F(ConfigPathTest, BackupProofDefaultsToFalseForOlderConfigFiles)
{
    const fs::path configPath = testHome_ / "autofilesync.json";
    writeFile(configPath, R"({
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

    ASSERT_TRUE(Config::instance().read(configPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].backupProof());
    Config::instance().reset();
}

// "Gelöschte Dateien im Ziel ebenfalls löschen" is persisted under the
// syncflags key "syncDeletions" (writeSyncItem/getSyncFlags). The option
// deletes user data, so a silent loss of the flag on save/load - or a silent
// gain of it - is the kind of defect that has to be caught here rather than
// in the field.
TEST_F(ConfigPathTest, DeleteInTargetRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.deleteInTarget());
    si.deleteInTarget(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].deleteInTarget());
    Config::instance().reset();
}

// The opposite direction matters just as much: an item saved with the option
// off must never come back with it on.
TEST_F(ConfigPathTest, DeleteInTargetStaysOffThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].deleteInTarget());
    Config::instance().reset();
}

// A config file written before the flag existed has no "syncDeletions" node.
// Loading it must default to off - defaulting to on would start deleting
// target files on the next sync of an item the user never opted in for.
TEST_F(ConfigPathTest, DeleteInTargetDefaultsToFalseForOlderConfigFiles)
{
    const fs::path configPath = testHome_ / "legacy_syncdeletions.json";
    writeFile(configPath, R"({
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

    ASSERT_TRUE(Config::instance().read(configPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].deleteInTarget());
    Config::instance().reset();
}

// "Leere Dateien synchronisieren" wird unter dem syncflags-Schluessel
// "syncEmptyFiles" abgelegt. Wie bei syncDeletions gilt: geht das Flag beim
// Speichern oder Laden verloren - oder kommt es ungefragt dazu -, aendert sich
// das Verhalten an den Daten des Benutzers.
TEST_F(ConfigPathTest, SyncEmptyFilesRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    ASSERT_FALSE(si.syncEmptyFiles());
    si.syncEmptyFiles(true);
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(items[0].syncEmptyFiles());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, SyncEmptyFilesStaysOffThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    SyncItem si(L"item", SyncItemType::Tree, L"/src", L"/dst", L"", Exclusions(),
        SyncFlags(false, false, false, false, true), CompareItems());
    Config::instance().add(si);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].syncEmptyFiles());
    Config::instance().reset();
}

// Eine Konfigurationsdatei aus der Zeit vor dem Flag kennt den Schluessel
// nicht. Sie muss mit ausgeschaltetem Flag laden - andernfalls faengt eine
// bestehende Installation nach dem Update ungefragt an, leere Dateien zu
// uebertragen und dabei womoeglich gefuellte Zieldateien zu ueberschreiben.
TEST_F(ConfigPathTest, SyncEmptyFilesDefaultsToFalseForOlderConfigFiles)
{
    const fs::path configPath = testHome_ / "legacy_syncemptyfiles.json";
    writeFile(configPath, R"({
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

    ASSERT_TRUE(Config::instance().read(configPath.wstring()));

    const auto items = Config::instance().syncitems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].syncEmptyFiles());
    Config::instance().reset();
}

TEST_F(ConfigPathTest, BackupProofPathRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().backupProofPath(L"/tmp/nachweise");
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_TRUE(Config::instance().settings().backupProofPath().empty());
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    EXPECT_EQ(Config::instance().settings().backupProofPath(), L"/tmp/nachweise");
    Config::instance().reset();
}

TEST_F(ConfigPathTest, RecycleBinPathRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    const fs::path binPath = testHome_ / "papierkorb";
    Config::instance().reset();
    RecycleBin::instance().binPath(binPath);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    RecycleBin::instance().binPath("");
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    EXPECT_EQ(RecycleBin::instance().binPath(), binPath);
    Config::instance().reset();
}

TEST_F(ConfigPathTest, DeleteModeRoundTripsThroughSaveAndLoad)
{
    const fs::path targetPath = testHome_ / "autofilesync.json";
    Config::instance().reset();
    Config::instance().settings().deleteMode(DeleteMode::Permanent);
    ASSERT_TRUE(Config::instance().write(targetPath.wstring()));

    Config::instance().reset();
    ASSERT_EQ(Config::instance().settings().deleteMode(), DeleteMode::RecycleBin);
    ASSERT_TRUE(Config::instance().read(targetPath.wstring()));

    EXPECT_EQ(Config::instance().settings().deleteMode(), DeleteMode::Permanent);
    Config::instance().reset();
}

// A config file written before the delete mode existed has no DeleteMode key:
// it must fall back to the recoverable mode rather than fail the Settings node.
TEST_F(ConfigPathTest, ConfigWithoutDeleteModeFallsBackToRecycleBin)
{
    const fs::path configPath = testHome_ / "legacy_deletemode.json";
    writeFile(configPath, R"({
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
    Config::instance().settings().deleteMode(DeleteMode::Permanent);

    ASSERT_TRUE(Config::instance().read(configPath.wstring()));

    EXPECT_EQ(Config::instance().settings().deleteMode(), DeleteMode::RecycleBin);
    EXPECT_TRUE(Config::instance().settings().showErrors());
    Config::instance().reset();
}

// A config file written before the recycle bin existed has no RecycleBinPath
// key: it must keep the configured path instead of blanking it, and must not
// take the rest of the Settings node down with it.
TEST_F(ConfigPathTest, ConfigWithoutRecycleBinPathKeepsCurrentPath)
{
    const fs::path configPath = testHome_ / "legacy_settings.json";
    writeFile(configPath, R"({
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
    const fs::path binPath = testHome_ / "papierkorb";
    RecycleBin::instance().binPath(binPath);

    ASSERT_TRUE(Config::instance().read(configPath.wstring()));

    EXPECT_EQ(RecycleBin::instance().binPath(), binPath);
    EXPECT_TRUE(Config::instance().settings().startMinimized());
    EXPECT_TRUE(Config::instance().settings().showErrors());
    Config::instance().reset();
}
