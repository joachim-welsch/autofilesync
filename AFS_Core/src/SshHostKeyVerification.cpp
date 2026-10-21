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
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <utility>

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
} // namespace

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

#else
#ifdef _WIN32

namespace {

std::string known_hosts_file_path()
{
    const char* profile = std::getenv("USERPROFILE");
    return profile ? std::string(profile) + "\\.ssh\\known_hosts" : "known_hosts";
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
    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND: // trust-on-first-use
        libssh2_knownhost_addc(known_hosts, host.c_str(), nullptr, key, key_len, nullptr, 0, key_mask, nullptr);
        libssh2_knownhost_writefile(known_hosts, known_hosts_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
        log_info(translation::translate("host_key_stored") + string_to_wstring(host));
        result = true;
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


namespace {
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
} // namespace

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
#endif
#endif

} // namespace AFS_Core
