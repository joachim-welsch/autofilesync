#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "SshDirInspector.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <format>

#ifdef _UNIX
    #include <libssh/sftp.h>
    #include <sys/stat.h>
#endif

#ifdef _UNIX
// ============================================================================
// UNIX/Linux-Implementierung mit libssh
// ============================================================================

SshDirInspector::SshDirInspector(const SshAccount& account)
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

SshDirInspector::~SshDirInspector()
{
    if (sftp_session_ptr) {
        sftp_free(sftp_session_ptr);
    }
    cleanup();
}

void SshDirInspector::cleanup()
{
    if (my_ssh_session) {
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        my_ssh_session = nullptr;
    }
}

RemoteDirInfo SshDirInspector::inspectDir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    info.exists = false;
    info.is_directory = false;
    info.path = remote_path;

    // Prüfen ob der Pfad existiert und ein Verzeichnis ist
    sftp_attributes path_attrs = sftp_stat(sftp_session_ptr, remote_path.c_str());
    if (path_attrs == nullptr) {
        info.error_message = "Pfad existiert nicht: " + remote_path;
        return info;
    }

    info.exists = true;
    info.is_directory = S_ISDIR(path_attrs->permissions);
    sftp_attributes_free(path_attrs);

    if (!info.is_directory) {
        info.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return info;
    }

    // Verzeichnis öffnen
    sftp_dir dir = sftp_opendir(sftp_session_ptr, remote_path.c_str());
    if (dir == nullptr) {
        info.error_message = "Fehler beim Öffnen des Verzeichnisses: " + 
            std::string(ssh_get_error(my_ssh_session));
        return info;
    }

    // Alle Einträge lesen
    sftp_attributes attributes;
    while ((attributes = sftp_readdir(sftp_session_ptr, dir)) != nullptr) {
        std::string name = attributes->name;
        
        // . und .. überspringen
        if (name == "." || name == "..") {
            sftp_attributes_free(attributes);
            continue;
        }

        RemoteDirEntry entry{};
        entry.name = name;
        entry.is_directory = S_ISDIR(attributes->permissions);
        entry.is_regular_file = S_ISREG(attributes->permissions);
        entry.is_symlink = S_ISLNK(attributes->permissions);
        entry.size_bytes = attributes->size;
        entry.raw_mtime = attributes->mtime;
        entry.permissions = attributes->permissions;

        // Konvertierung der Zeit in lesbaren String
        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        info.entries.push_back(entry);
        sftp_attributes_free(attributes);
    }

    // Prüfen ob ein Fehler aufgetreten ist oder EOF erreicht wurde
    if (!sftp_dir_eof(dir)) {
        info.error_message = "Fehler beim Lesen des Verzeichnisses: " + 
            std::string(ssh_get_error(my_ssh_session));
    }

    sftp_closedir(dir);
    return info;
}

std::string SshDirInspector::getHomeDir()
{
    char* resolved = sftp_canonicalize_path(sftp_session_ptr, ".");
    if (!resolved) {
        throw std::runtime_error("Home-Verzeichnis konnte nicht ermittelt werden: " +
            std::string(ssh_get_error(my_ssh_session)));
    }
    std::string home(resolved);
    ssh_string_free_char(resolved);
    return home;
}

RemoteDirInfo SshDirInspector::inspectDirRecursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    result.path = remote_path;
    result.exists = false;
    result.is_directory = false;

    // Prüfen ob der Pfad existiert und ein Verzeichnis ist
    sftp_attributes path_attrs = sftp_stat(sftp_session_ptr, remote_path.c_str());
    if (path_attrs == nullptr) {
        result.error_message = "Pfad existiert nicht: " + remote_path;
        return result;
    }

    result.exists = true;
    result.is_directory = S_ISDIR(path_attrs->permissions);
    sftp_attributes_free(path_attrs);

    if (!result.is_directory) {
        result.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return result;
    }

    inspectDirRecursiveImpl(remote_path, result);
    return result;
}

void SshDirInspector::inspectDirRecursiveImpl(const std::string& remote_path, RemoteDirInfo& result)
{
    sftp_dir dir = sftp_opendir(sftp_session_ptr, remote_path.c_str());
    if (dir == nullptr) {
        result.error_message = "Fehler beim Öffnen des Verzeichnisses: " + remote_path;
        return;
    }

    sftp_attributes attributes;
    while ((attributes = sftp_readdir(sftp_session_ptr, dir)) != nullptr) {
        std::string name = attributes->name;
        
        // . und .. überspringen
        if (name == "." || name == "..") {
            sftp_attributes_free(attributes);
            continue;
        }

        std::string full_path = remote_path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        RemoteDirEntry entry{};
        entry.name = full_path;  // Bei rekursiv: vollständiger Pfad
        entry.is_directory = S_ISDIR(attributes->permissions);
        entry.is_regular_file = S_ISREG(attributes->permissions);
        entry.is_symlink = S_ISLNK(attributes->permissions);
        entry.size_bytes = attributes->size;
        entry.raw_mtime = attributes->mtime;
        entry.permissions = attributes->permissions;

        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        result.entries.push_back(entry);

        // Rekursiv in Unterverzeichnisse gehen
        if (entry.is_directory && !entry.is_symlink) {
            inspectDirRecursiveImpl(full_path, result);
        }

        sftp_attributes_free(attributes);
    }

    // NULL von sftp_readdir bedeutet EOF oder Fehler - nur sftp_dir_eof
    // unterscheidet beides; eine Teilliste darf nicht als vollständig gelten.
    if (!sftp_dir_eof(dir))
        result.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;

    sftp_closedir(dir);
}

#else
// ============================================================================
// Windows-Implementierung mit libssh2
// ============================================================================

#ifdef _WIN32

// Makros zur Prüfung der Permissions-Flags
#define CHECK_IS_DIRECTORY(permissions) \
    (((permissions) & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFDIR)

#define CHECK_IS_REGULAR_FILE(permissions) \
    (((permissions) & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFREG)

#define CHECK_IS_SYMLINK(permissions) \
    (((permissions) & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFLNK)

SshDirInspector::SshDirInspector(const SshAccount& account)
    : account_(account)
{
    // 1. Winsock initialisieren
    WSADATA wsadata;
    int rc = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (rc != 0) {
        throw std::runtime_error("WSAStartup fehlgeschlagen");
    }

    // 2. Socket erstellen und verbinden
    struct sockaddr_in sin;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("Fehler beim Erstellen des Sockets");
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = inet_addr(account_.host().c_str());
    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* host = gethostbyname(account_.host().c_str());
        if (!host) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Hostname konnte nicht aufgelöst werden: " + account_.host());
        }
        memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    }

    if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Verbindung zum Server fehlgeschlagen");
    }

    // 3. libssh2 initialisieren (einmalig für den gesamten Prozess)
    ensureLibssh2Initialized();

    // 4. SSH Session erstellen
    my_ssh_session = libssh2_session_init();
    if (!my_ssh_session) {
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Fehler beim Erstellen der SSH Session");
    }
    applySshConnectTimeout(my_ssh_session);

    // 5. SSH Handshake
    rc = libssh2_session_handshake(my_ssh_session, sock);
    if (rc != 0) {
        cleanup();
        throw std::runtime_error("SSH Handshake fehlgeschlagen");
    }

    if (!verifySshHostKey(my_ssh_session, account_.host())) {
        cleanup();
        throw std::runtime_error("Host-Key-Prüfung fehlgeschlagen: mögliche Man-in-the-Middle-Attacke.");
    }

    // 6. Authentifizierung
    rc = libssh2_userauth_password(my_ssh_session, account_.user().c_str(), account_.password().c_str());
    if (rc != 0) {
        cleanup();
        throw std::runtime_error("Authentifizierung fehlgeschlagen");
    }

    // 7. SFTP Session öffnen
    sftp_session_ptr = libssh2_sftp_init(my_ssh_session);
    if (!sftp_session_ptr) {
        cleanup();
        throw std::runtime_error("SFTP Initialisierung fehlgeschlagen");
    }
}

SshDirInspector::~SshDirInspector()
{
    if (sftp_session_ptr) {
        libssh2_sftp_shutdown(sftp_session_ptr);
        sftp_session_ptr = nullptr;
    }
    cleanup();
}

void SshDirInspector::cleanup()
{
    if (my_ssh_session) {
        libssh2_session_disconnect(my_ssh_session, "Normal Shutdown");
        libssh2_session_free(my_ssh_session);
        my_ssh_session = nullptr;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
}

RemoteDirInfo SshDirInspector::inspectDir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    info.exists = false;
    info.is_directory = false;
    info.path = remote_path;

    // Prüfen ob der Pfad existiert und ein Verzeichnis ist
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr, remote_path.c_str(), &attrs);
    if (rc != 0) {
        info.error_message = "Pfad existiert nicht: " + remote_path;
        return info;
    }

    info.exists = true;
    info.is_directory = CHECK_IS_DIRECTORY(attrs.permissions);

    if (!info.is_directory) {
        info.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return info;
    }

    // Verzeichnis öffnen
    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr, remote_path.c_str());
    if (!dir_handle) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session, &err_msg, nullptr, 0);
        info.error_message = "Fehler beim Öffnen des Verzeichnisses: " + std::string(err_msg);
        return info;
    }

    // Alle Einträge lesen
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;

        // . und .. überspringen
        if (name == "." || name == "..") {
            continue;
        }

        RemoteDirEntry entry{};
        entry.name = name;
        entry.is_directory = CHECK_IS_DIRECTORY(entry_attrs.permissions);
        entry.is_regular_file = CHECK_IS_REGULAR_FILE(entry_attrs.permissions);
        entry.is_symlink = CHECK_IS_SYMLINK(entry_attrs.permissions);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        // Konvertierung der Zeit in lesbaren String
        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        info.entries.push_back(entry);
    }

    // 0 ist EOF, negativ ist ein Fehler mitten im Listing.
    if (read_rc < 0)
        info.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;

    libssh2_sftp_closedir(dir_handle);
    return info;
}

std::string SshDirInspector::getHomeDir()
{
    char buffer[1024];
    int rc = libssh2_sftp_realpath(sftp_session_ptr, ".", buffer, sizeof(buffer));
    if (rc < 0) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session, &err_msg, nullptr, 0);
        throw std::runtime_error("Home-Verzeichnis konnte nicht ermittelt werden: " + std::string(err_msg));
    }
    return std::string(buffer, rc);
}

RemoteDirInfo SshDirInspector::inspectDirRecursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    result.path = remote_path;
    result.exists = false;
    result.is_directory = false;

    // Prüfen ob der Pfad existiert und ein Verzeichnis ist
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr, remote_path.c_str(), &attrs);
    if (rc != 0) {
        result.error_message = "Pfad existiert nicht: " + remote_path;
        return result;
    }

    result.exists = true;
    result.is_directory = CHECK_IS_DIRECTORY(attrs.permissions);

    if (!result.is_directory) {
        result.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return result;
    }

    inspectDirRecursiveImpl(remote_path, result);
    return result;
}

void SshDirInspector::inspectDirRecursiveImpl(const std::string& remote_path, RemoteDirInfo& result)
{
    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr, remote_path.c_str());
    if (!dir_handle) {
        result.error_message = "Fehler beim Öffnen des Verzeichnisses: " + remote_path;
        return;
    }

    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;

        // . und .. überspringen
        if (name == "." || name == "..") {
            continue;
        }

        std::string full_path = remote_path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        RemoteDirEntry entry{};
        entry.name = full_path;  // Bei rekursiv: vollständiger Pfad
        entry.is_directory = CHECK_IS_DIRECTORY(entry_attrs.permissions);
        entry.is_regular_file = CHECK_IS_REGULAR_FILE(entry_attrs.permissions);
        entry.is_symlink = CHECK_IS_SYMLINK(entry_attrs.permissions);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        result.entries.push_back(entry);

        // Rekursiv in Unterverzeichnisse gehen
        if (entry.is_directory && !entry.is_symlink) {
            inspectDirRecursiveImpl(full_path, result);
        }
    }

    // 0 ist EOF, negativ ist ein Fehler mitten im Listing.
    if (read_rc < 0)
        result.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;

    libssh2_sftp_closedir(dir_handle);
}

#endif // _WIN32
#endif // _UNIX
