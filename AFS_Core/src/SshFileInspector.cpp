#include "pch.h"
#include "SshFileInspector.h"
#include "SyncFileSsh.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "util.h"

#ifdef _UNIX
    #include <libssh/sftp.h>
#else
    #ifdef _WIN32
        #include <winsock2.h>
        #include <ws2tcpip.h>
        // MinGW: Link with -lws2_32 -lssh2 in Makefile/CMakeLists.txt
    #endif
#endif

// Hilfsfunktion: time_t in ISO-String konvertieren (plattformunabhängig)
static std::string timeToIso(time_t t) {
    struct tm timeinfo;
    #ifdef _WIN32
        localtime_s(&timeinfo, &t);
    #else
        localtime_r(&t, &timeinfo);
    #endif
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#ifdef _UNIX
// ============================================================================
// UNIX/Linux-Implementierung mit libssh
// ============================================================================

SshFileInspector::SshFileInspector(const SshAccount& account)
    : account_(account)
{
    // 1. SSH Session erstellen
    my_ssh_session = ssh_new();
    if (my_ssh_session == nullptr) {
        throw std::runtime_error("Fehler beim Erstellen der SSH Session.");
    }

    // 2. Optionen setzen
    ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, account_.host().c_str());
    ssh_options_set(my_ssh_session, SSH_OPTIONS_USER, account_.user().c_str());
    applySshConnectTimeout(my_ssh_session);

    // 3. Verbinden
    int rc = ssh_connect(my_ssh_session);
    if (rc != SSH_OK) {
        std::string err = ssh_get_error(my_ssh_session);
        ssh_free(my_ssh_session);
        throw std::runtime_error("Verbindungsfehler: " + err);
    }

    if (!verifySshHostKey(my_ssh_session, account_.host())) {
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        throw std::runtime_error("Host-Key-Prüfung fehlgeschlagen: mögliche Man-in-the-Middle-Attacke.");
    }

    // 4. Authentifizierung mit Passwort
    rc = ssh_userauth_password(my_ssh_session, nullptr, account_.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        std::string err = ssh_get_error(my_ssh_session);
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        throw std::runtime_error("Authentifizierungsfehler: " + err);
    }

    // 5. SFTP Session initialisieren
    sftp_session_ptr = sftp_new(my_ssh_session);
    if (sftp_session_ptr == nullptr) {
        cleanup();
        throw std::runtime_error("Fehler beim Erstellen der SFTP Session.");
    }

    rc = sftp_init(sftp_session_ptr);
    if (rc != SSH_OK) {
        sftp_free(sftp_session_ptr);
        cleanup();
        throw std::runtime_error("Fehler beim Initialisieren von SFTP.");
    }
}

SshFileInspector::~SshFileInspector()
{
    if (sftp_session_ptr) {
        sftp_free(sftp_session_ptr);
    }
    cleanup();
}

RemoteFileInfo SshFileInspector::inspectFile(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";

    // Configured paths may use either separator (settings.cpp stores them as
    // entered); the remote sftp server only understands slashes.
    const std::string sftp_path = win_to_sftp_path(remote_path);
    sftp_attributes attributes = sftp_stat(sftp_session_ptr, sftp_path.c_str());
    if (attributes == nullptr) {
        // Only a confirmed "no such file" may report exists=false: the caller
        // (readSshFileInfo) treats that as safe-to-delete for deleteInTarget.
        // Any other failure (permission denied, dropped session) must not be
        // mistaken for an absent file - throw into the caller's error path.
        const int err = sftp_get_error(sftp_session_ptr);
        if (err == SSH_FX_NO_SUCH_FILE || err == SSH_FX_NO_SUCH_PATH)
            return info;
        throw std::runtime_error("SFTP-Stat fehlgeschlagen (Fehlercode " + std::to_string(err) + "): " + sftp_path);
    }
    info.exists = true;
    info.size_bytes = attributes->size;
    info.raw_mtime = attributes->mtime;
    info.last_modified_iso = timeToIso(info.raw_mtime);
    sftp_attributes_free(attributes);
    return info;
}

void SshFileInspector::cleanup()
{
    if (my_ssh_session) {
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        my_ssh_session = nullptr;
    }
}

#else
#ifdef _WIN32
// ============================================================================
// Windows-Implementierung mit libssh2
// ============================================================================

static void init_winsock() {
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsadata;
        int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (err != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        initialized = true;
    }
}

SshFileInspector::SshFileInspector(const SshAccount& account) 
    : account_(account) {
    
    init_winsock();
    ensureLibssh2Initialized();

    // Hostname auflösen
    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(account.host().c_str(), "22", &hints, &result) != 0) {
        throw std::runtime_error("Failed to resolve hostname: " + account.host());
    }
    
    // Socket erstellen und verbinden
    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        throw std::runtime_error("Failed to create socket");
    }
    
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        closesocket(sock);
        sock = INVALID_SOCKET;
        throw std::runtime_error("Failed to connect to host");
    }
    freeaddrinfo(result);
    
    // SSH-Session erstellen
    my_ssh_session = libssh2_session_init();
    if (!my_ssh_session) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        throw std::runtime_error("Failed to create SSH session");
    }
    applySshConnectTimeout(my_ssh_session);

    int rc = libssh2_session_handshake(my_ssh_session, sock);
    if (rc) {
        cleanup();
        throw std::runtime_error("SSH handshake failed");
    }

    if (!verifySshHostKey(my_ssh_session, account.host())) {
        cleanup();
        throw std::runtime_error("Host key verification failed: possible man-in-the-middle attack.");
    }

    // Authentifizierung
    rc = libssh2_userauth_password(my_ssh_session, 
                                   account.user().c_str(), 
                                   account.password().c_str());
    if (rc) {
        cleanup();
        throw std::runtime_error("Authentication failed");
    }
    
    // SFTP-Session erstellen
    sftp_session_ptr = libssh2_sftp_init(my_ssh_session);
    if (!sftp_session_ptr) {
        cleanup();
        throw std::runtime_error("Failed to initialize SFTP session");
    }
}

SshFileInspector::~SshFileInspector() {
    cleanup();
}

void SshFileInspector::cleanup() {
    if (sftp_session_ptr) {
        libssh2_sftp_shutdown(sftp_session_ptr);
        sftp_session_ptr = nullptr;
    }
    if (my_ssh_session) {
        libssh2_session_disconnect(my_ssh_session, "Normal shutdown");
        libssh2_session_free(my_ssh_session);
        my_ssh_session = nullptr;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

RemoteFileInfo SshFileInspector::inspectFile(const std::string& remote_path) {
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";
    
    if (!sftp_session_ptr) {
        return info;
    }
    auto sftp_path = remote_path;
#ifdef WIN32
    // Nur die Schreibweise anpassen ("C:\dir" -> "/C:/dir", Backslashes zu
    // Schraegstrichen). Frueher lief hier zusaetzlich getRemotePath() darueber
    // und schnitt am ersten Doppelpunkt ab - bei einem Pfad mit
    // Laufwerksbuchstaben wurde aus "C:\temp\x" so "/temp/x". Dass das
    // ueberhaupt funktionierte, lag allein daran, dass der Windows-SFTP-Server
    // "/temp" auf dem Systemlaufwerk sucht. Der Parameter ist der entfernte
    // Pfad ohne Kontopraefix - so uebergeben ihn alle Aufrufer.
    sftp_path = win_to_sftp_path(remote_path);
#endif
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr, sftp_path.c_str(), &attrs);

    if (rc != 0) {
        // Only a confirmed "no such file" may report exists=false - see the
        // UNIX variant above. Other errors (permission denied, session error)
        // must throw into the caller's error path.
        const unsigned long err = libssh2_sftp_last_error(sftp_session_ptr);
        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL
            && (err == LIBSSH2_FX_NO_SUCH_FILE || err == LIBSSH2_FX_NO_SUCH_PATH)) {
            info.exists = false;
            return info;
        }
        throw std::runtime_error("SFTP stat failed (error " + std::to_string(err) + "): " + remote_path);
    }
#ifdef WIN32
    if (remote_path.empty()) {
        info.exists = false;
        return info;
	}
#endif
    info.exists = true;
    info.size_bytes = attrs.filesize;
    info.raw_mtime = static_cast<time_t>(attrs.mtime);
    info.last_modified_iso = timeToIso(info.raw_mtime);
    
    return info;
}

#else
// ============================================================================
// Fallback-Implementierung für nicht unterstützte Plattformen
// ============================================================================

SshFileInspector::SshFileInspector(const SshAccount& account) 
    : account_(account) {
    throw std::runtime_error("SSH not supported on this platform");
}

SshFileInspector::~SshFileInspector() {}

void SshFileInspector::cleanup() {}

RemoteFileInfo SshFileInspector::inspectFile(const std::string& remote_path) {
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";
    return info;
}

#endif // _WIN32
#endif // _UNIX
