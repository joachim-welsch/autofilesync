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
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <openssl/evp.h>
#include <functional>
#include <filesystem>
#include <mutex>
#include <utility>
#include <vector>
#ifdef _WIN32
    #include "Libssh2Init.h"
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

namespace AFS_Core {

using util::string_to_wstring;

namespace {
std::pair<std::string, std::string> type_and_blob(const std::string& line)  // [SshHostKeyVerification.md#verify_pinned_host_key]
{
    const size_t first = line.find(' ');
    if (first == std::string::npos)
        return {};
    const size_t second = line.find(' ', first + 1);
    return {line.substr(0, first), line.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1)};
}

std::vector<unsigned char> decoded_base64(const std::string& text)
{
    std::vector<unsigned char> out(3 * (text.size() / 4) + 3);
    const int len = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(text.data()), static_cast<int>(text.size()));
    if (len < 0)
        return {};
    size_t padding = 0;
    for (auto it = text.rbegin(); it != text.rend() && *it == '='; ++it)
        ++padding;
    out.resize(static_cast<size_t>(len) - padding);
    return out;
}

std::string unpadded_base64(const unsigned char* data, size_t length)
{
    std::vector<unsigned char> out(4 * ((length + 2) / 3) + 1);
    const int len = EVP_EncodeBlock(out.data(), data, static_cast<int>(length));
    std::string text(reinterpret_cast<char*>(out.data()), static_cast<size_t>(len));
    while (!text.empty() && text.back() == '=')
        text.pop_back();
    return text;
}
} // namespace

std::string host_key_fingerprint(const std::string& host_key_line)  // [SshHostKeyVerification.md#host_key_fingerprint]
{
    const std::vector<unsigned char> blob = decoded_base64(type_and_blob(host_key_line).second);
    if (blob.empty())
        return {};
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(blob.data(), blob.size(), digest, &length, EVP_sha256(), nullptr) != 1)
        return {};
    return "SHA256:" + unpadded_base64(digest, length);
}

namespace {
std::mutex decision_mutex;
UnknownHostKeyDecision unknown_key_decision;

[[maybe_unused]] bool unknown_key_accepted(const std::string& host, const std::string& fingerprint)  // [SshHostKeyVerification.md#unknown-host-key]
{
    UnknownHostKeyDecision decision;
    {
        std::lock_guard<std::mutex> lock(decision_mutex);
        decision = unknown_key_decision;
    }
    return decision && decision(host, fingerprint);
}
} // namespace

void on_unknown_host_key(UnknownHostKeyDecision decision)
{
    std::lock_guard<std::mutex> lock(decision_mutex);
    unknown_key_decision = std::move(decision);
}

#ifdef _UNIX

bool verify_ssh_host_key(ssh_session session, const std::string& host)
{
    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        return true;
    case SSH_KNOWN_HOSTS_NOT_FOUND: // no known_hosts file yet: trust-on-first-use
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (ssh_session_update_known_hosts(session) != SSH_OK) {
            log_error(translation::translate("host_key_not_storable") + string_to_wstring(host));
            return false;
        }
        log_info(translation::translate("host_key_stored") + string_to_wstring(host));
        return true;
    case SSH_KNOWN_HOSTS_CHANGED:
        log_error(translation::translate("host_key_changed") + string_to_wstring(host));
        return false;
    default:
        log_error(translation::translate("host_key_verification_failed_for") + string_to_wstring(host));
        return false;
    }
}


bool verify_pinned_host_key(ssh_session session, const std::string& expected_line, const std::string& host)  // [SshHostKeyVerification.md#verify_pinned_host_key]
{
    ssh_key key = nullptr;
    if (ssh_get_server_publickey(session, &key) != SSH_OK) {
        log_error(translation::translate("host_key_not_readable") + string_to_wstring(host));
        return false;
    }
    char* base64 = nullptr;
    const bool exported = ssh_pki_export_pubkey_base64(key, &base64) == SSH_OK;
    const std::string presented_type = ssh_key_type_to_char(ssh_key_type(key));
    const std::string presented_blob = exported && base64 ? base64 : "";
    ssh_string_free_char(base64);
    ssh_key_free(key);
    const auto [expected_type, expected_blob] = type_and_blob(expected_line);
    if (exported && presented_type == expected_type && presented_blob == expected_blob)
        return true;
    log_error(translation::translate("host_key_changed") + string_to_wstring(host));
    return false;
}

std::optional<std::string> fetch_ssh_host_key(const SshAccount& account, std::string& error)  // [SshHostKeyVerification.md#fetch_ssh_host_key]
{
    ssh_session session = ssh_new();
    if (!session) {
        error = translation::translate_utf8("ssh_session_create_error");
        return std::nullopt;
    }
    ssh_options_set(session, SSH_OPTIONS_HOST, account.host().c_str());
    const unsigned int port = account.port();
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    apply_ssh_connect_timeout(session);
    std::optional<std::string> line;
    if (ssh_connect(session) == SSH_OK) {
        ssh_key key = nullptr;
        if (ssh_get_server_publickey(session, &key) == SSH_OK) {
            char* base64 = nullptr;
            if (ssh_pki_export_pubkey_base64(key, &base64) == SSH_OK)
                line = std::string(ssh_key_type_to_char(ssh_key_type(key))) + " " + base64;
            ssh_string_free_char(base64);
            ssh_key_free(key);
        }
        ssh_disconnect(session);
    }
    if (!line)
        error = ssh_get_error(session);
    ssh_free(session);
    return line;
}

#else
#ifdef _WIN32

namespace {

std::string known_hosts_file_path()
{
    const char* profile = std::getenv("USERPROFILE");
    return profile ? std::string(profile) + "\\.ssh\\known_hosts" : "known_hosts";
}

std::string base64_of(const unsigned char* data, size_t length)
{
    static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < length; i += 3) {
        const unsigned int chunk = (data[i] << 16) | (i + 1 < length ? data[i + 1] << 8 : 0) | (i + 2 < length ? data[i + 2] : 0);
        out += ALPHABET[(chunk >> 18) & 63];
        out += ALPHABET[(chunk >> 12) & 63];
        out += i + 1 < length ? ALPHABET[(chunk >> 6) & 63] : '=';
        out += i + 2 < length ? ALPHABET[chunk & 63] : '=';
    }
    return out;
}

std::string fingerprint_of(LIBSSH2_SESSION* session)  // [SshHostKeyVerification.md#fingerprint_of]
{
    constexpr size_t SHA256_LENGTH = 32;
    const char* hash = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!hash)
        return {};
    std::string text = base64_of(reinterpret_cast<const unsigned char*>(hash), SHA256_LENGTH);
    text.erase(text.find_last_not_of('=') + 1);
    return "SHA256:" + text;
}

std::string known_host_name(const std::string& host, unsigned short port)  // [SshHostKeyVerification.md#known_host_name]
{
    constexpr unsigned short DEFAULT_PORT = 22;
    return port == DEFAULT_PORT ? host : "[" + host + "]:" + std::to_string(port);
}

bool store_confirmed_key(LIBSSH2_KNOWNHOSTS* known_hosts, const std::string& path, const std::string& name, const char* key, size_t key_len, int key_mask)
{
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    return libssh2_knownhost_addc(known_hosts, name.c_str(), nullptr, key, key_len, nullptr, 0, key_mask, nullptr) == 0
        && libssh2_knownhost_writefile(known_hosts, path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH) == 0;
}

int known_host_key_type_mask(int hostkey_type)  // [SshHostKeyVerification.md#known_host_key_type_mask]
{
    switch (hostkey_type) {
    case LIBSSH2_HOSTKEY_TYPE_RSA: return LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    case LIBSSH2_HOSTKEY_TYPE_DSS: return LIBSSH2_KNOWNHOST_KEY_SSHDSS;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: return LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: return LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: return LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
    case LIBSSH2_HOSTKEY_TYPE_ED25519: return LIBSSH2_KNOWNHOST_KEY_ED25519;
    default: return LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
    }
}

} // namespace

bool verify_ssh_host_key(LIBSSH2_SESSION* session, const std::string& host, unsigned short port)
{
    size_t key_len = 0;
    int key_type = 0;
    const char* key = libssh2_session_hostkey(session, &key_len, &key_type);
    if (!key) {
        log_error(translation::translate("host_key_not_readable") + string_to_wstring(host));
        return false;
    }

    LIBSSH2_KNOWNHOSTS* known_hosts = libssh2_knownhost_init(session);
    if (!known_hosts) {
        log_error(translation::translate("known_hosts_store_failed"));
        return false;
    }
    const std::string known_hosts_path = known_hosts_file_path();
    libssh2_knownhost_readfile(known_hosts, known_hosts_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    const int key_mask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | known_host_key_type_mask(key_type);
    libssh2_knownhost* match = nullptr;
    const int check = libssh2_knownhost_checkp(known_hosts, host.c_str(), port, key, key_len, key_mask, &match);

    bool result = false;
    switch (check) {
    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        result = true;
        break;
    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        result = unknown_key_accepted(known_host_name(host, port), fingerprint_of(session));  // [SshHostKeyVerification.md#unknown-host-key]
        if (!result)
            log_error(translation::translate("host_key_unconfirmed", {string_to_wstring(host)}));
        else if (store_confirmed_key(known_hosts, known_hosts_path, known_host_name(host, port), key, key_len, key_mask))
            log_info(translation::translate("host_key_stored") + string_to_wstring(host));
        else
            log_error(translation::translate("host_key_not_storable") + string_to_wstring(host));
        break;
    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        log_error(translation::translate("host_key_changed") + string_to_wstring(host));
        result = false;
        break;
    default:
        log_error(translation::translate("host_key_verification_failed_for") + string_to_wstring(host));
        result = false;
    }

    libssh2_knownhost_free(known_hosts);
    return result;
}


bool verify_pinned_host_key(LIBSSH2_SESSION* session, const std::string& expected_line, const std::string& host)  // [SshHostKeyVerification.md#verify_pinned_host_key]
{
    size_t key_len = 0;
    int key_type = 0;
    const char* key = libssh2_session_hostkey(session, &key_len, &key_type);
    if (!key) {
        log_error(translation::translate("host_key_not_readable") + string_to_wstring(host));
        return false;
    }
    const auto [expected_type, expected_blob] = type_and_blob(expected_line);
    if (!expected_type.empty() && base64_of(reinterpret_cast<const unsigned char*>(key), key_len) == expected_blob)
        return true;  // the blob names its own type, so the type needs no separate mapping
    log_error(translation::translate("host_key_changed") + string_to_wstring(host));
    return false;
}

namespace {
const char* host_key_type_name(int hostkey_type)  // [SshHostKeyVerification.md#fetch_ssh_host_key]
{
    switch (hostkey_type) {
    case LIBSSH2_HOSTKEY_TYPE_RSA: return "ssh-rsa";
    case LIBSSH2_HOSTKEY_TYPE_DSS: return "ssh-dss";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: return "ecdsa-sha2-nistp256";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: return "ecdsa-sha2-nistp384";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: return "ecdsa-sha2-nistp521";
    case LIBSSH2_HOSTKEY_TYPE_ED25519: return "ssh-ed25519";
    default: return "unknown";
    }
}

SOCKET connected_socket(const SshAccount& account, std::string& error)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(account.host().c_str(), std::to_string(account.port()).c_str(), &hints, &result) != 0) {
        error = translation::translate_utf8("host_name_not_resolvable") + account.host();
        return INVALID_SOCKET;
    }
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock != INVALID_SOCKET && connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) != 0) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if (sock == INVALID_SOCKET)
        error = translation::translate_utf8("server_connect_failed");
    return sock;
}
} // namespace

std::optional<std::string> fetch_ssh_host_key(const SshAccount& account, std::string& error)  // [SshHostKeyVerification.md#fetch_ssh_host_key]
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        error = translation::translate_utf8("wsastartup_failed");
        return std::nullopt;
    }
    ensure_libssh2_initialized();
    const SOCKET sock = connected_socket(account, error);
    if (sock == INVALID_SOCKET)
        return std::nullopt;
    std::optional<std::string> line;
    LIBSSH2_SESSION* session = libssh2_session_init();
    if (session) {
        apply_ssh_connect_timeout(session);
        if (libssh2_session_handshake(session, sock) == 0) {
            size_t key_len = 0;
            int key_type = 0;
            if (const char* key = libssh2_session_hostkey(session, &key_len, &key_type))
                line = std::string(host_key_type_name(key_type)) + " " + base64_of(reinterpret_cast<const unsigned char*>(key), key_len);
            libssh2_session_disconnect(session, "Normal Shutdown");
        }
        if (!line) {
            char* message = nullptr;
            libssh2_session_last_error(session, &message, nullptr, 0);
            error = message ? message : "";
        }
        libssh2_session_free(session);
    } else
        error = translation::translate_utf8("ssh_session_create_error");
    closesocket(sock);
    return line;
}
#endif
#endif

} // namespace AFS_Core
