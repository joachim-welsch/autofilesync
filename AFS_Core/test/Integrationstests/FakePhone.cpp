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
#include "FakePhone.h"
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

namespace tu {

namespace {

std::string json_string(const std::string& text)
{
    std::string out = "\"";
    for (const char c : text)
        out += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
    return out + "\"";
}

X509* certificate_from(const std::string& pem)
{
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    X509* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return certificate;
}

EVP_PKEY* key_from(const std::string& pem)
{
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return key;
}

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        if (!line.empty())
            lines.push_back(line);
    return lines;
}

} // namespace

std::string FakePhone::Reply::type(size_t line) const
{
    return field("type", line);
}

std::string FakePhone::Reply::field(const std::string& path, size_t line) const
{
    if (line >= lines.size())
        return {};
    boost::property_tree::ptree tree;
    try {
        std::istringstream in(lines[line]);
        boost::property_tree::read_json(in, tree);
    }
    catch (const std::exception&) {
        return {};
    }
    return tree.get<std::string>(path, "");
}

FakePhone::FakePhone(const std::filesystem::path& identity_dir)
    : identity_(*AFS_Core::DeviceIdentity::load_or_create(identity_dir))  // [FakePhone.md#identity]
{}

FakePhone::Reply FakePhone::send(uint16_t port, const std::string& line, bool with_certificate) const  // [FakePhone.md#send]
{
    Reply reply;
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    if (with_certificate) {
        X509* certificate = certificate_from(identity_.certificate_pem());
        EVP_PKEY* key = key_from(identity_.key_pem());
        SSL_CTX_use_certificate(ctx, certificate);
        SSL_CTX_use_PrivateKey(ctx, key);
        X509_free(certificate);
        EVP_PKEY_free(key);
    }
    BIO* bio = BIO_new_ssl_connect(ctx);
    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    const std::string address = "127.0.0.1:" + std::to_string(port);
    BIO_set_conn_hostname(bio, address.c_str());
    if (BIO_do_connect(bio) > 0 && BIO_do_handshake(bio) > 0) {
        reply.connected = true;
        X509* server = SSL_get1_peer_certificate(ssl);
        reply.server_fingerprint = AFS_Core::certificate_fingerprint(server);
        X509_free(server);
        const std::string request = line + "\n";
        BIO_write(bio, request.data(), static_cast<int>(request.size()));
        BIO_flush(bio);
        std::string text;
        char chunk[4096];
        for (int n; (n = BIO_read(bio, chunk, sizeof chunk)) > 0;)
            text.append(chunk, static_cast<size_t>(n));
        reply.lines = split_lines(text);
    }
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    return reply;
}

std::string FakePhone::pair_line(const std::string& token, const std::string& id, const std::string& name, const std::string& host_key)
{
    return "{\"type\":\"pair\",\"token\":" + json_string(token) + ",\"device\":{\"id\":" + json_string(id) + ",\"name\":" + json_string(name)
        + ",\"sshHostKey\":" + json_string(host_key) + ",\"sftpPort\":52382},\"appVersion\":\"test\"}";
}

std::string FakePhone::hello_line(const std::string& id, const SftpOffer& sftp)
{
    return "{\"type\":\"hello\",\"deviceId\":" + json_string(id) + ",\"sftpPort\":" + std::to_string(sftp.port) + ",\"sftpUser\":" + json_string(sftp.user)
        + ",\"sftpPassword\":" + json_string(sftp.password) + ",\"serving\":" + (sftp.serving ? "true" : "false") + ",\"appVersion\":\"test\"}";
}

} // namespace tu
