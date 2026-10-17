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
#include "SftpConnection.h"
#include "SshConnectTimeout.h"
#include "SshAccount.h"
#include "SshUnreachable.h"
#include <libssh/libssh.h>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace AFS_Core;
using namespace std::chrono;

namespace {

constexpr seconds CONFIGURED_TIMEOUT(15);  // SSH_CONNECT_TIMEOUT_SECONDS in SshConnectTimeout.cpp
constexpr seconds MARGIN(10);

class SilentServer {  // [SshConnectTimeoutTests.md#SilentServer]
    int listener_ = socket(AF_INET, SOCK_STREAM, 0);
    uint16_t port_ = 0;
public:
    SilentServer()
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || listen(listener_, 4) != 0)
            throw std::runtime_error("SilentServer: cannot listen on 127.0.0.1");
        socklen_t size = sizeof address;
        getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size);
        port_ = ntohs(address.sin_port);
    }
    ~SilentServer() { close(listener_); }
    SilentServer(const SilentServer&) = delete;
    SilentServer& operator=(const SilentServer&) = delete;
    uint16_t port() const { return port_; }
};

SshAccount account_for(const std::string& host)
{
    SshAccount account;
    account.set_name("stumm");
    account.set_user("nobody");
    account.set_host(host);
    account.set_password("irrelevant");
    return account;
}

seconds seconds_until_unreachable(const SshAccount& account)  // [SshConnectTimeoutTests.md#seconds_until_unreachable]
{
    const auto start = steady_clock::now();
    EXPECT_THROW(SftpConnection connection(account), SshUnreachable);
    return duration_cast<seconds>(steady_clock::now() - start);
}

} // namespace

TEST(SshConnectTimeoutTests, AServerThatNeverAnswersIsGivenUpWithinTheTimeout)  // [SshConnectTimeoutTests.md#AServerThatNeverAnswersIsGivenUpWithinTheTimeout]
{
    const SilentServer server;
    ssh_session session = ssh_new();
    ASSERT_NE(session, nullptr);
    const char* host = "127.0.0.1";
    int port = server.port();
    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    apply_ssh_connect_timeout(session);

    const auto start = steady_clock::now();
    const int result = ssh_connect(session);
    const seconds elapsed = duration_cast<seconds>(steady_clock::now() - start);
    ssh_free(session);

    EXPECT_NE(result, SSH_OK);
    EXPECT_LT(elapsed, CONFIGURED_TIMEOUT + MARGIN) << "die Verbindung blockierte laenger als die Zeitbegrenzung erlaubt";
    EXPECT_GE(elapsed, seconds(2)) << "ohne Antwort des Servers muss die Zeitbegrenzung greifen, nicht ein sofortiger Fehler";
}

TEST(SshConnectTimeoutTests, ABlackholeAddressIsGivenUpWithinTheTimeout)  // [SshConnectTimeoutTests.md#ABlackholeAddressIsGivenUpWithinTheTimeout]
{
    const seconds elapsed = seconds_until_unreachable(account_for("192.0.2.1"));

    EXPECT_LT(elapsed, CONFIGURED_TIMEOUT + MARGIN) << "die Verbindung blockierte laenger als die Zeitbegrenzung erlaubt";
    if (elapsed < seconds(2))
        GTEST_SKIP() << "dieses Netz lehnt die Adresse sofort ab, die Zeitbegrenzung kam nicht zum Zug";
}
#endif
