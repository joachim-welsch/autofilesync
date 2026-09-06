#include "pch.h"
#include "SshHostKeyVerification.h"
#include "Logging.h"
#include "util.h"

namespace AFS_Core {

using util::string_to_wstring;

#ifdef _UNIX

bool verifySshHostKey(ssh_session session, const std::string& host)
{
    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        return true;
    case SSH_KNOWN_HOSTS_NOT_FOUND: // no known_hosts file yet: trust-on-first-use
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (ssh_session_update_known_hosts(session) != SSH_OK) {
            LOG_ERROR(L"Host-Key konnte nicht gespeichert werden: " + string_to_wstring(host));
            return false;
        }
        LOG_INFO(L"Neuer Host-Key gespeichert für: " + string_to_wstring(host));
        return true;
    case SSH_KNOWN_HOSTS_CHANGED:
        LOG_ERROR(L"Host-Key hat sich geändert, mögliche Man-in-the-Middle-Attacke: " + string_to_wstring(host));
        return false;
    default:
        LOG_ERROR(L"Host-Key-Prüfung fehlgeschlagen für: " + string_to_wstring(host));
        return false;
    }
}

#else
#ifdef _WIN32

namespace {

std::string knownHostsFilePath()
{
    const char* profile = std::getenv("USERPROFILE");
    return profile ? std::string(profile) + "\\.ssh\\known_hosts" : "known_hosts";
}

// Maps libssh2's hostkey type to the matching LIBSSH2_KNOWNHOST_KEY_* flag
// so libssh2_knownhost_checkp compares against the right key type.
int knownHostKeyTypeMask(int hostkeyType)
{
    switch (hostkeyType) {
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

bool verifySshHostKey(LIBSSH2_SESSION* session, const std::string& host, unsigned short port)
{
    size_t keyLen = 0;
    int keyType = 0;
    const char* key = libssh2_session_hostkey(session, &keyLen, &keyType);
    if (!key) {
        LOG_ERROR(L"Host-Key konnte nicht gelesen werden: " + string_to_wstring(host));
        return false;
    }

    LIBSSH2_KNOWNHOSTS* knownHosts = libssh2_knownhost_init(session);
    if (!knownHosts) {
        LOG_ERROR(L"Known-Hosts-Speicher konnte nicht erstellt werden");
        return false;
    }
    const std::string knownHostsPath = knownHostsFilePath();
    libssh2_knownhost_readfile(knownHosts, knownHostsPath.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    const int keyMask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | knownHostKeyTypeMask(keyType);
    libssh2_knownhost* match = nullptr;
    const int check = libssh2_knownhost_checkp(knownHosts, host.c_str(), port, key, keyLen, keyMask, &match);

    bool result = false;
    switch (check) {
    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        result = true;
        break;
    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND: // trust-on-first-use
        libssh2_knownhost_addc(knownHosts, host.c_str(), nullptr, key, keyLen, nullptr, 0, keyMask, nullptr);
        libssh2_knownhost_writefile(knownHosts, knownHostsPath.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
        LOG_INFO(L"Neuer Host-Key gespeichert für: " + string_to_wstring(host));
        result = true;
        break;
    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        LOG_ERROR(L"Host-Key hat sich geändert, mögliche Man-in-the-Middle-Attacke: " + string_to_wstring(host));
        result = false;
        break;
    default:
        LOG_ERROR(L"Host-Key-Prüfung fehlgeschlagen für: " + string_to_wstring(host));
        result = false;
    }

    libssh2_knownhost_free(knownHosts);
    return result;
}

#endif
#endif

} // namespace AFS_Core
