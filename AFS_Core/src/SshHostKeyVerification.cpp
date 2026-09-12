#include "pch.h"
#include "SshHostKeyVerification.h"
#include "Logging.h"
#include "Util.h"

namespace AFS_Core {

using util::string_to_wstring;

#ifdef _UNIX

bool verify_ssh_host_key(ssh_session session, const std::string& host)
{
    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        return true;
    case SSH_KNOWN_HOSTS_NOT_FOUND: // no known_hosts file yet: trust-on-first-use
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (ssh_session_update_known_hosts(session) != SSH_OK) {
            log_error(L"Host-Key konnte nicht gespeichert werden: " + string_to_wstring(host));
            return false;
        }
        log_info(L"Neuer Host-Key gespeichert für: " + string_to_wstring(host));
        return true;
    case SSH_KNOWN_HOSTS_CHANGED:
        log_error(L"Host-Key hat sich geändert, mögliche Man-in-the-Middle-Attacke: " + string_to_wstring(host));
        return false;
    default:
        log_error(L"Host-Key-Prüfung fehlgeschlagen für: " + string_to_wstring(host));
        return false;
    }
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
        log_error(L"Host-Key konnte nicht gelesen werden: " + string_to_wstring(host));
        return false;
    }

    LIBSSH2_KNOWNHOSTS* known_hosts = libssh2_knownhost_init(session);
    if (!known_hosts) {
        log_error(L"Known-Hosts-Speicher konnte nicht erstellt werden");
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
        log_info(L"Neuer Host-Key gespeichert für: " + string_to_wstring(host));
        result = true;
        break;
    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        log_error(L"Host-Key hat sich geändert, mögliche Man-in-the-Middle-Attacke: " + string_to_wstring(host));
        result = false;
        break;
    default:
        log_error(L"Host-Key-Prüfung fehlgeschlagen für: " + string_to_wstring(host));
        result = false;
    }

    libssh2_knownhost_free(known_hosts);
    return result;
}

#endif
#endif

} // namespace AFS_Core
