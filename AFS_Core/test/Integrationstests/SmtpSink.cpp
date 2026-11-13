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
#include "SmtpSink.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <algorithm>
#include <cctype>
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace tu {

std::string base64_decode(const std::string& encoded)
{
    std::string decoded(encoded.size(), '\0');
    const int length = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(&decoded[0]),
        reinterpret_cast<const unsigned char*>(encoded.data()), static_cast<int>(encoded.size()));
    if (length < 0)
        return {};
    const auto padding = std::count(encoded.end() - std::min<size_t>(2, encoded.size()), encoded.end(), '=');
    decoded.resize(static_cast<size_t>(length) - static_cast<size_t>(padding));  // EVP_DecodeBlock counts the padding as bytes
    return decoded;
}

std::string SmtpSink::Mail::subject() const
{
    const std::string marker = "Subject: =?UTF-8?B?";
    const auto start = data.find(marker);
    if (start == std::string::npos)
        return {};
    const auto end = data.find("?=", start + marker.size());
    return base64_decode(data.substr(start + marker.size(), end - start - marker.size()));
}

std::string SmtpSink::Mail::body() const
{
    const auto separator = data.find("\r\n\r\n");
    if (separator == std::string::npos)
        return {};
    std::string encoded;
    for (const char c : data.substr(separator + 4))
        if (c != '\r' && c != '\n')
            encoded += c;
    return base64_decode(encoded);
}

#ifndef _WIN32

void SmtpSink::create_certificate()  // [SmtpSink.md#StartTls]
{
    EVP_PKEY* key = EVP_EC_gen("P-256");
    X509* certificate = X509_new();
    X509_set_version(certificate, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
    X509_gmtime_adj(X509_getm_notBefore(certificate), -3600);
    X509_gmtime_adj(X509_getm_notAfter(certificate), 3600);
    X509_set_pubkey(certificate, key);
    X509_NAME* name = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("SmtpSink"), -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);
    X509_EXTENSION* alt_name = X509V3_EXT_conf_nid(nullptr, &context, NID_subject_alt_name, "IP:127.0.0.1");
    X509_add_ext(certificate, alt_name, -1);
    X509_EXTENSION_free(alt_name);
    X509_sign(certificate, key, EVP_sha256());
    BIO* pem = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(pem, certificate);
    char* text = nullptr;
    const long length = BIO_get_mem_data(pem, &text);
    certificate_pem_.assign(text, static_cast<size_t>(length));
    BIO_free(pem);
    tls_context_ = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate(tls_context_, certificate);
    SSL_CTX_use_PrivateKey(tls_context_, key);
    X509_free(certificate);
    EVP_PKEY_free(key);
}

SmtpSink::SmtpSink(Security security)
{
    if (security == Security::StartTls)
        create_certificate();
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;  // [SmtpSink.md#ephemeral-port]
    if (bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || listen(listener_, 4) != 0)
        throw std::runtime_error("SmtpSink: cannot listen on 127.0.0.1");
    socklen_t size = sizeof address;
    getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size);
    port_ = ntohs(address.sin_port);
    thread_ = std::thread(&SmtpSink::serve, this);
}

SmtpSink::~SmtpSink()
{
    stop_ = true;
    thread_.join();
    close(listener_);
    if (tls_context_)
        SSL_CTX_free(tls_context_);
}

AFS_Core::SmtpAccount SmtpSink::account() const
{
    AFS_Core::SmtpAccount account;
    account.server = L"127.0.0.1";
    account.port = port_;
    account.from = L"afs@example.org";
    return account;
}

void SmtpSink::accept_login(const Login& login)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accepted_login_ = login;
}

std::vector<SmtpSink::Login> SmtpSink::logins() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return logins_;
}

void SmtpSink::reply_to_rcpt(const std::string& reply)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rcpt_reply_ = reply;
}

std::vector<SmtpSink::Mail> SmtpSink::mails() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return mails_;
}

std::vector<std::string> SmtpSink::commands() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_;
}

bool SmtpSink::wait_for_connections(int count, std::chrono::milliseconds timeout) const
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (connections_ < count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return connections_ >= count;
}

void SmtpSink::serve()  // [SmtpSink.md#serve]
{
    while (!stop_) {
        pollfd waiting{listener_, POLLIN, 0};
        if (poll(&waiting, 1, 50) <= 0)
            continue;
        const int client = accept(listener_, nullptr, nullptr);
        if (client < 0)
            continue;
        session(client);
        if (tls_) {
            SSL_free(tls_);
            tls_ = nullptr;
        }
        close(client);
        ++connections_;
    }
}

void SmtpSink::record(const std::string& command)
{
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back(command);
}

static std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

static std::string address_of(const std::string& command)
{
    const auto open = command.find('<');
    const auto close = command.find('>', open == std::string::npos ? 0 : open);
    return open == std::string::npos || close == std::string::npos ? std::string() : command.substr(open + 1, close - open - 1);
}

bool SmtpSink::start_tls(int client, std::string& buffer)  // [SmtpSink.md#StartTls]
{
    send(client, "220 go ahead");
    buffer.clear();
    tls_ = SSL_new(tls_context_);
    SSL_set_fd(tls_, client);
    return SSL_accept(tls_) > 0;
}

void SmtpSink::login(int client, std::string& buffer)
{
    Login login;
    std::string line;
    send(client, "334 VXNlcm5hbWU6");
    if (!read_line(client, buffer, line))
        return;
    login.user = base64_decode(line);
    send(client, "334 UGFzc3dvcmQ6");
    if (!read_line(client, buffer, line))
        return;
    login.password = base64_decode(line);
    std::lock_guard<std::mutex> lock(mutex_);
    logins_.push_back(login);
    send(client, login == accepted_login_ ? "235 welcome" : "535 wrong user or password");
}

void SmtpSink::session(int client)
{
    std::string buffer, line;
    Mail mail;
    if (!send(client, "220 sink ready"))
        return;
    while (!stop_ && read_line(client, buffer, line)) {
        record(line);
        const std::string command = upper(line);
        if (command.rfind("EHLO", 0) == 0 || command.rfind("HELO", 0) == 0)
            send(client, "250-sink\r\n250 OK");
        else if (command == "STARTTLS" && tls_context_ && !tls_) {
            if (!start_tls(client, buffer))
                return;
        }
        else if (command == "STARTTLS")
            send(client, "502 STARTTLS not supported");
        else if (command == "AUTH LOGIN" && tls_)
            login(client, buffer);
        else if (command.rfind("AUTH", 0) == 0)
            send(client, "503 AUTH not available");
        else if (command.rfind("MAIL FROM:", 0) == 0) {
            mail.from = address_of(line);
            send(client, "250 OK");
        }
        else if (command.rfind("RCPT TO:", 0) == 0) {
            mail.to = address_of(line);
            std::lock_guard<std::mutex> lock(mutex_);
            send(client, rcpt_reply_);
        }
        else if (command == "DATA") {
            send(client, "354 go ahead");
            mail.data.clear();
            while (read_line(client, buffer, line) && line != ".")
                mail.data += line + "\r\n";
            std::lock_guard<std::mutex> lock(mutex_);
            mails_.push_back(mail);
            send(client, "250 queued");
        }
        else if (command == "QUIT") {
            send(client, "221 bye");
            return;
        }
        else
            send(client, "500 unknown command");
    }
}

bool SmtpSink::send(int client, const std::string& reply) const
{
    const std::string data = reply + "\r\n";
    if (tls_)
        return SSL_write(tls_, data.data(), static_cast<int>(data.size())) == static_cast<int>(data.size());
    return ::send(client, data.data(), data.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(data.size());
}

bool SmtpSink::read_line(int client, std::string& buffer, std::string& line) const
{
    for (;;) {
        const auto end = buffer.find("\r\n");
        if (end != std::string::npos) {
            line = buffer.substr(0, end);
            buffer.erase(0, end + 2);
            return true;
        }
        pollfd waiting{client, POLLIN, 0};
        if (!(tls_ && SSL_pending(tls_) > 0) && poll(&waiting, 1, 50) <= 0) {  // [SmtpSink.md#tls_]
            if (stop_)
                return false;
            continue;
        }
        char chunk[1024];
        const ssize_t received = tls_ ? SSL_read(tls_, chunk, sizeof chunk) : recv(client, chunk, sizeof chunk, 0);
        if (received <= 0)
            return false;
        buffer.append(chunk, static_cast<size_t>(received));
    }
}

#endif

} // namespace tu
