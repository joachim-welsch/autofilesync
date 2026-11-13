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
#pragma once
#include "SmtpMailer.h"
#include <openssl/ssl.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tu {
std::string base64_decode(const std::string& encoded);

class SmtpSink {  // [SmtpSink.md#SmtpSink]
public:
    struct Mail {
        std::string from;
        std::string to;
        std::string data;  // headers and body as received, up to the final "."
        std::string subject() const;  // decoded from the encoded word
        std::string body() const;  // decoded from the base64 lines
    };
    enum class Security { Plain, StartTls };  // [SmtpSink.md#StartTls]
    struct Login {
        std::string user;
        std::string password;
        bool operator==(const Login&) const = default;
    };
    explicit SmtpSink(Security security = Security::Plain);
    ~SmtpSink();
    SmtpSink(const SmtpSink&) = delete;
    SmtpSink& operator=(const SmtpSink&) = delete;
    uint16_t port() const { return port_; }
    AFS_Core::SmtpAccount account() const;  // [SmtpSink.md#account]
    void reply_to_rcpt(const std::string& reply);
    void accept_login(const Login& login);  // [SmtpSink.md#StartTls]
    const std::string& certificate_pem() const { return certificate_pem_; }
    std::vector<Login> logins() const;
    std::vector<Mail> mails() const;
    std::vector<std::string> commands() const;
    int connections() const { return connections_; }
    bool wait_for_connections(int count, std::chrono::milliseconds timeout) const;
private:
    void serve();
    void session(int client);
    bool send(int client, const std::string& reply) const;
    bool read_line(int client, std::string& buffer, std::string& line) const;
    void record(const std::string& command);
    void create_certificate();
    bool start_tls(int client, std::string& buffer);
    void login(int client, std::string& buffer);
    SSL_CTX* tls_context_ = nullptr;
    SSL* tls_ = nullptr;  // [SmtpSink.md#tls_]
    std::string certificate_pem_;
    Login accepted_login_;
    std::vector<Login> logins_;
    int listener_ = -1;
    uint16_t port_ = 0;
    std::atomic_bool stop_{false};
    std::atomic_int connections_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::string rcpt_reply_ = "250 OK";
    std::vector<Mail> mails_;
    std::vector<std::string> commands_;
};
} // namespace tu
