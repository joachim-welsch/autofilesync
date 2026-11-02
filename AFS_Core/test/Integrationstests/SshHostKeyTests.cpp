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
#include "SshHostKeyVerification.h"
#include "SshTest.h"
#include "ScopedLog.h"
#include "Translation.h"
#include "TestPaths.h"
#include <libssh/libssh.h>
#include <filesystem>
#include <fstream>
#include <string>

using namespace AFS_Core;
using namespace SSHTest;

namespace {

namespace fs = std::filesystem;

class Session {  // [SshHostKeyTests.md#Session]
    ssh_session session_ = ssh_new();
public:
    explicit Session(const fs::path& known_hosts)
    {
        const std::string known_hosts_path = known_hosts.string();
        ssh_options_set(session_, SSH_OPTIONS_HOST, SSH_HOST);
        ssh_options_set(session_, SSH_OPTIONS_USER, SSH_USER);
        ssh_options_set(session_, SSH_OPTIONS_KNOWNHOSTS, known_hosts_path.c_str());
        connected_ = ssh_connect(session_) == SSH_OK;
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session()
    {
        if (connected_)
            ssh_disconnect(session_);
        ssh_free(session_);
    }
    bool connected_ = false;
    ssh_session get() const { return session_; }
};

class SshHostKeyTest : public ::testing::Test {
protected:
    const fs::path dir_ = fs::temp_directory_path() / ("autofilesync_hostkey_" + std::to_string(TestPaths::process_id()));
    const fs::path known_hosts_ = dir_ / "known_hosts";

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::permissions(dir_, fs::perms::owner_all, fs::perm_options::add, ec);
        fs::remove_all(dir_, ec);
    }

    const tu::ScopedLog log_{dir_ / "verify.log", LogLevel::Info};  // [SshHostKeyTests.md#log-names-the-branch]

    bool logged(const char* msgid) const
    {
        return read(log_.path()).find(translation::translate_utf8(msgid)) != std::string::npos;
    }

    bool verify(const fs::path& known_hosts) const
    {
        Session session(known_hosts);
        EXPECT_TRUE(session.connected_) << "keine Verbindung zum Testserver";
        return session.connected_ && verify_ssh_host_key(session.get(), SSH_HOST);
    }

    std::string read(const fs::path& file) const
    {
        std::ifstream in(file);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    std::string entry_with_a_different_key_of_the_servers_type() const  // [SshHostKeyTests.md#different-key-same-type]
    {
        Session session(known_hosts_);
        ssh_key server_key = nullptr;
        EXPECT_EQ(ssh_get_server_publickey(session.get(), &server_key), SSH_OK);
        const ssh_keytypes_e type = ssh_key_type(server_key);
        ssh_key_free(server_key);
        ssh_key other_key = nullptr;
        EXPECT_EQ(ssh_pki_generate(type, key_parameter(type), &other_key), SSH_OK);
        char* base64 = nullptr;
        EXPECT_EQ(ssh_pki_export_pubkey_base64(other_key, &base64), SSH_OK);
        const std::string entry = std::string(SSH_HOST) + " " + ssh_key_type_to_char(type) + " " + base64 + "\n";
        ssh_string_free_char(base64);
        ssh_key_free(other_key);
        return entry;
    }

    static int key_parameter(ssh_keytypes_e type)
    {
        switch (type) {
        case SSH_KEYTYPE_RSA: return 2048;
        case SSH_KEYTYPE_ECDSA_P256: return 256;
        case SSH_KEYTYPE_ECDSA_P384: return 384;
        case SSH_KEYTYPE_ECDSA_P521: return 521;
        default: return 0;
        }
    }
};

} // namespace

TEST_F(SshHostKeyTest, UnknownHostIsTrustedOnFirstUseAndStored)  // [SshHostKeyTests.md#UnknownHostIsTrustedOnFirstUseAndStored]
{
    ASSERT_FALSE(fs::exists(known_hosts_));

    EXPECT_TRUE(verify(known_hosts_));

    ASSERT_TRUE(fs::exists(known_hosts_)) << "der Schluessel wurde nicht gespeichert";
    EXPECT_TRUE(logged("host_key_stored"));
    EXPECT_NE(read(known_hosts_).find(SSH_HOST), std::string::npos) << read(known_hosts_);
}

TEST_F(SshHostKeyTest, StoredHostKeyIsAcceptedWithoutChangingTheFile)  // [SshHostKeyTests.md#StoredHostKeyIsAcceptedWithoutChangingTheFile]
{
    ASSERT_TRUE(verify(known_hosts_));
    const std::string stored = read(known_hosts_);

    EXPECT_TRUE(verify(known_hosts_));

    EXPECT_EQ(read(known_hosts_), stored) << "ein bekannter Schluessel darf nicht erneut angehaengt werden";
}

TEST_F(SshHostKeyTest, ChangedHostKeyIsRefused)  // [SshHostKeyTests.md#ChangedHostKeyIsRefused]
{
    const std::string other = entry_with_a_different_key_of_the_servers_type();
    std::ofstream(known_hosts_) << other;

    EXPECT_FALSE(verify(known_hosts_)) << "ein geaenderter Hostschluessel muss die Verbindung verhindern";

    EXPECT_EQ(read(known_hosts_), other) << "der fremde Eintrag darf weder ersetzt noch ergaenzt werden";
    EXPECT_TRUE(logged("host_key_changed")) << "abgelehnt, aber nicht aus dem Grund \"Schluessel geaendert\"";
}

TEST_F(SshHostKeyTest, UnwritableKnownHostsFailsClosed)  // [SshHostKeyTests.md#UnwritableKnownHostsFailsClosed]
{
    const fs::path locked_dir = dir_ / "gesperrt";
    fs::create_directories(locked_dir);
    fs::permissions(locked_dir, fs::perms::owner_read | fs::perms::owner_exec);

    EXPECT_FALSE(verify(locked_dir / "known_hosts")) << "ohne gespeicherten Schluessel darf die Verbindung nicht zustande kommen";

    EXPECT_FALSE(fs::exists(locked_dir / "known_hosts"));
    EXPECT_TRUE(logged("host_key_not_storable"));
}

TEST_F(SshHostKeyTest, FetchedHostKeyIsTheOneTheServerPresents)  // [SshHostKeyTests.md#FetchedHostKeyIsTheOneTheServerPresents]
{
    SshAccount account;
    account.set_host(SSH_HOST);
    std::string error;

    const auto line = fetch_ssh_host_key(account, error);

    ASSERT_TRUE(line.has_value()) << error;
    EXPECT_NE(line->find(' '), std::string::npos) << *line;
    EXPECT_EQ(host_key_fingerprint(*line).rfind("SHA256:", 0), 0u) << host_key_fingerprint(*line);
    EXPECT_EQ(host_key_fingerprint(*line).find('='), std::string::npos) << "OpenSSH prints the fingerprint without padding";
    Session session(known_hosts_);
    ASSERT_TRUE(session.connected_);
    EXPECT_TRUE(verify_pinned_host_key(session.get(), *line, SSH_HOST)) << "the fetched line must pass the pinning check";
    EXPECT_FALSE(fs::exists(known_hosts_)) << "fetching a key must not store it";
}

TEST_F(SshHostKeyTest, FetchFromAClosedPortReportsTheReason)
{
    SshAccount account;
    account.set_host(SSH_HOST);
    account.set_port(1);
    std::string error;

    EXPECT_FALSE(fetch_ssh_host_key(account, error).has_value());

    EXPECT_FALSE(error.empty());
}

TEST(SshHostKeyFingerprint, MalformedLineHasNoFingerprint)
{
    EXPECT_EQ(host_key_fingerprint(""), "");
    EXPECT_EQ(host_key_fingerprint("ssh-ed25519"), "");
    EXPECT_EQ(host_key_fingerprint("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGb0b1pQ8iKNvjN0VcTq6Yj9Bqz2m7u5K8fNvHq6Lk3z"),
        "SHA256:" + host_key_fingerprint("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGb0b1pQ8iKNvjN0VcTq6Yj9Bqz2m7u5K8fNvHq6Lk3z comment").substr(7));
}
#endif
