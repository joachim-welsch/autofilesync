#include "pch.h"
#include "SyncFileSsh.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include <fstream>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _UNIX
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#else
#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
    #pragma comment(lib, "ws2_32.lib")
#endif
#endif
#include "config.h"
#include "util.h"
#include "Logging.h"
#include "Synchronize.h"

using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

#ifdef _TEST
std::atomic<int> sshIsDirectoryCallCount{0};
#endif

std::string win_to_sftp_path(const std::string& win)
{
    std::string p = win;
    // Backslashes -> Slashes
    std::replace(p.begin(), p.end(), '\\', '/');
    // "C:/..." -> "/C:/..."
    if (p.size() >= 2 && std::isalpha(p[0]) && p[1] == ':')
        p = "/" + p;
    return p;
}

namespace {
    const size_t TenMB{ 10 * 1024 * 1024 };

    // SFTP write calls may legitimately write fewer bytes than requested
    // (short write); only a negative return is an error. Treating a short
    // write as failure made large uploads abort sporadically.
#ifdef _UNIX
    bool sftpWriteAll(sftp_file file, const char* data, size_t length)
    {
        while (length > 0) {
            ssize_t written = sftp_write(file, data, length);
            if (written <= 0)
                return false;
            data += written;
            length -= static_cast<size_t>(written);
        }
        return true;
    }
#endif
#ifdef _WIN32
    bool sftpWriteAll(LIBSSH2_SFTP_HANDLE* file, const char* data, size_t length)
    {
        while (length > 0) {
            ssize_t written = libssh2_sftp_write(file, data, length);
            if (written <= 0)
                return false;
            data += written;
            length -= static_cast<size_t>(written);
        }
        return true;
    }
#endif

#ifdef _WIN32
    class WinsockInit {
    public:
        WinsockInit() {
            WSADATA wsadata;
            WSAStartup(MAKEWORD(2, 0), &wsadata);
        }
        ~WinsockInit() {
            WSACleanup();
        }
    };

    static WinsockInit winsockInit;

    // Resolves hostname or IP address to sockaddr_in
    // Returns true on success, false on failure
    bool resolveHost(const std::string& host, sockaddr_in& sin) {
        sin = {};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(22);
#ifdef _WIN32
        // Versuche zuerst, als IP-Adresse zu parsen (nun mit inet_pton)
        int ptonResult = inet_pton(AF_INET, host.c_str(), &sin.sin_addr);
        if (ptonResult == 1) {
            return true;
        }
#else
        // First try to parse as IP address
        sin.sin_addr.s_addr = inet_addr(host.c_str());
        if (sin.sin_addr.s_addr != INADDR_NONE) {
            return true;
        }
#endif
        // If not a valid IP, try to resolve hostname
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        int rc = getaddrinfo(host.c_str(), "22", &hints, &result);
        if (rc != 0 || result == nullptr) {
            return false;
        }

        sin = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
        freeaddrinfo(result);
        return true;
    }
#endif
}

std::optional<SshAccount> getAccountFromPath(const wstring& path)
{
    auto colonPos = path.find(L':');
    if (colonPos == wstring::npos || colonPos == 0) {
        return std::nullopt;
    }
    wstring wsshName = path.substr(0, colonPos);
    std::string sshName = wstring_to_string(wsshName);
    return Config::instance().findSshAccountByName(sshName);
}

std::string getRemotePath(const wstring& path)
{
    auto colonPos = path.find(L':');
    if (colonPos == wstring::npos) {
        return wstring_to_string(path);
    }
    return wstring_to_string(path.substr(colonPos + 1));
}

#ifdef _UNIX
CopyResult copySshToFs(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copySshToFs: " + src + L" -> " + dest);
    
    // SSH-Account aus dem Pfad extrahieren
    auto accountOpt = getAccountFromPath(src);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + src);
        return CopyResult::Error;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(src);
    
    // SSH-Session erstellen
    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return CopyResult::Error;
    }
    
    // Optionen setzen
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);
    
    // Verbinden
    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // Authentifizierung
    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // SFTP Session
    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // Remote-Datei öffnen
    sftp_file remoteFile = sftp_open(sftpSession, remotePath.c_str(), O_RDONLY, 0);
    if (remoteFile == nullptr) {
        LOG_ERROR(L"Fehler beim Öffnen der Remote-Datei: " + string_to_wstring(remotePath));
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }
    
    // Lokale Datei zum Schreiben öffnen
    std::string destPath = wstring_to_string(dest);
    std::ofstream localFile(destPath, std::ios::binary);
    if (!localFile.is_open()) {
        LOG_ERROR(L"Fehler beim Öffnen der lokalen Datei: " + dest);
        sftp_close(remoteFile);
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }
    
    // Datei kopieren
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    ssize_t bytesRead;
    CopyResult result = CopyResult::OK;
    
    while ((bytesRead = sftp_read(remoteFile, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        localFile.write(buffer.data(), bytesRead);
        if (!localFile.good()) {
            LOG_ERROR(L"Fehler beim Schreiben der lokalen Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytesRead < 0) {
        LOG_ERROR(L"Fehler beim Lesen der Remote-Datei");
        result = CopyResult::Error;
    }
    
    // Aufräumen
    localFile.close();
    sftp_close(remoteFile);
    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);
    
    return result;
}
#else
#ifdef _WIN32
CopyResult copySshToFs(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copySshToFs: " + src + L" -> " + dest);
    
    auto accountOpt = getAccountFromPath(src);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + src);
        return CopyResult::Error;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(src);

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return CopyResult::Error;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return CopyResult::Error;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler zum Host: " + string_to_wstring(account.host()));
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return CopyResult::Error;
    }
    applySshConnectTimeout(session);
    
    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP_HANDLE* remoteFile = libssh2_sftp_open(sftpSession, remotePath.c_str(),
        LIBSSH2_FXF_READ, 0);
    if (!remoteFile) {
        LOG_ERROR(L"Fehler beim Öffnen der Remote-Datei: " + string_to_wstring(remotePath));
        libssh2_sftp_shutdown(sftpSession);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    
    std::string destPath = wstring_to_string(dest);
    std::ofstream localFile(destPath, std::ios::binary);
    if (!localFile.is_open()) {
        LOG_ERROR(L"Fehler beim Öffnen der lokalen Datei: " + dest);
        libssh2_sftp_close(remoteFile);
        libssh2_sftp_shutdown(sftpSession);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    ssize_t bytesRead;
    CopyResult result = CopyResult::OK;
    
    while ((bytesRead = libssh2_sftp_read(remoteFile, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        localFile.write(buffer.data(), bytesRead);
        if (!localFile.good()) {
            LOG_ERROR(L"Fehler beim Schreiben der lokalen Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytesRead < 0) {
        LOG_ERROR(L"Fehler beim Lesen der Remote-Datei");
        result = CopyResult::Error;
    }
    
    // Aufräumen
    localFile.close();
    libssh2_sftp_close(remoteFile);
    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return result;
}
#endif
#endif

#ifdef _UNIX
CopyResult copySshToSsh(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copySshToSsh: " + src + L" -> " + dest);
    
    // SSH-Accounts aus den Pfaden extrahieren
    auto srcAccountOpt = getAccountFromPath(src);
    if (!srcAccountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für Quelle: " + src);
        return CopyResult::Error;
    }
    const SshAccount& srcAccount = srcAccountOpt.value();
    std::string srcRemotePath = getRemotePath(src);
    
    auto destAccountOpt = getAccountFromPath(dest);
    if (!destAccountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für Ziel: " + dest);
        return CopyResult::Error;
    }
    const SshAccount& destAccount = destAccountOpt.value();
    std::string destRemotePath = getRemotePath(dest);
    
    // Quell-SSH-Session erstellen
    ssh_session srcSshSession = ssh_new();
    if (srcSshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der Quell-SSH Session");
        return CopyResult::Error;
    }
    
    ssh_options_set(srcSshSession, SSH_OPTIONS_HOST, srcAccount.host().c_str());
    ssh_options_set(srcSshSession, SSH_OPTIONS_USER, srcAccount.user().c_str());
    applySshConnectTimeout(srcSshSession);
    
    int rc = ssh_connect(srcSshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler (Quelle): " + string_to_wstring(ssh_get_error(srcSshSession)));
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(srcSshSession, srcAccount.host())) {
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }

    rc = ssh_userauth_password(srcSshSession, nullptr, srcAccount.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler (Quelle): " + string_to_wstring(ssh_get_error(srcSshSession)));
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    sftp_session srcSftpSession = sftp_new(srcSshSession);
    if (srcSftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der Quell-SFTP Session");
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    rc = sftp_init(srcSftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler (Quelle)");
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    // Ziel-SSH-Session erstellen
    ssh_session destSshSession = ssh_new();
    if (destSshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der Ziel-SSH Session");
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    ssh_options_set(destSshSession, SSH_OPTIONS_HOST, destAccount.host().c_str());
    ssh_options_set(destSshSession, SSH_OPTIONS_USER, destAccount.user().c_str());
    applySshConnectTimeout(destSshSession);
    
    rc = ssh_connect(destSshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler (Ziel): " + string_to_wstring(ssh_get_error(destSshSession)));
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(destSshSession, destAccount.host())) {
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }

    rc = ssh_userauth_password(destSshSession, nullptr, destAccount.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler (Ziel): " + string_to_wstring(ssh_get_error(destSshSession)));
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    sftp_session destSftpSession = sftp_new(destSshSession);
    if (destSftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der Ziel-SFTP Session");
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    rc = sftp_init(destSftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler (Ziel)");
        sftp_free(destSftpSession);
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    // Quell-Datei öffnen
    sftp_file srcFile = sftp_open(srcSftpSession, srcRemotePath.c_str(), O_RDONLY, 0);
    if (srcFile == nullptr) {
        LOG_ERROR(L"Fehler beim Öffnen der Quell-Datei: " + string_to_wstring(srcRemotePath));
        sftp_free(destSftpSession);
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    // Ziel-Datei öffnen
    sftp_file destFile = sftp_open(destSftpSession, destRemotePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (destFile == nullptr) {
        LOG_ERROR(L"Fehler beim Öffnen der Ziel-Datei: " + string_to_wstring(destRemotePath));
        sftp_close(srcFile);
        sftp_free(destSftpSession);
        ssh_disconnect(destSshSession);
        ssh_free(destSshSession);
        sftp_free(srcSftpSession);
        ssh_disconnect(srcSshSession);
        ssh_free(srcSshSession);
        return CopyResult::Error;
    }
    
    // Datei kopieren
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    ssize_t bytesRead;
    CopyResult result = CopyResult::OK;
    
    while ((bytesRead = sftp_read(srcFile, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        if (!sftpWriteAll(destFile, buffer.data(), static_cast<size_t>(bytesRead))) {
            LOG_ERROR(L"Fehler beim Schreiben der Ziel-Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytesRead < 0) {
        LOG_ERROR(L"Fehler beim Lesen der Quell-Datei");
        result = CopyResult::Error;
    }
    
    // Aufräumen
    sftp_close(destFile);
    sftp_close(srcFile);
    sftp_free(destSftpSession);
    ssh_disconnect(destSshSession);
    ssh_free(destSshSession);
    sftp_free(srcSftpSession);
    ssh_disconnect(srcSshSession);
    ssh_free(srcSshSession);
    
    return result;
}
#else
#ifdef _WIN32
CopyResult copySshToSsh(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copySshToSsh: " + src + L" -> " + dest);
    
    auto srcAccountOpt = getAccountFromPath(src);
    if (!srcAccountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für Quelle: " + src);
        return CopyResult::Error;
    }
    const SshAccount& srcAccount = srcAccountOpt.value();
    std::string srcRemotePath = getRemotePath(src);
    
    auto destAccountOpt = getAccountFromPath(dest);
    if (!destAccountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für Ziel: " + dest);
        return CopyResult::Error;
    }
    const SshAccount& destAccount = destAccountOpt.value();
    std::string destRemotePath = getRemotePath(dest);

    ensureLibssh2Initialized();
    SOCKET srcSock = socket(AF_INET, SOCK_STREAM, 0);
    if (srcSock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Quell-Sockets");
        return CopyResult::Error;
    }

    sockaddr_in srcSin{};
    if (!resolveHost(srcAccount.host(), srcSin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Quell-Hostnamens: " + string_to_wstring(srcAccount.host()));
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (connect(srcSock, (struct sockaddr*)&srcSin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler (Quelle)");
        closesocket(srcSock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* srcSession = libssh2_session_init();
    if (!srcSession) {
        LOG_ERROR(L"Fehler beim Erstellen der Quell-SSH Session");
        closesocket(srcSock);
        return CopyResult::Error;
    }
    applySshConnectTimeout(srcSession);
    
    if (libssh2_session_handshake(srcSession, srcSock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler (Quelle)");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(srcSession, srcAccount.host())) {
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(srcSession, srcAccount.user().c_str(), srcAccount.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler (Quelle)");
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP* srcSftpSession = libssh2_sftp_init(srcSession);
    if (!srcSftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der Quell-SFTP Session");
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    SOCKET destSock = socket(AF_INET, SOCK_STREAM, 0);
    if (destSock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Ziel-Sockets");
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    sockaddr_in destSin{};
    if (!resolveHost(destAccount.host(), destSin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Ziel-Hostnamens: " + string_to_wstring(destAccount.host()));
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (connect(destSock, (struct sockaddr*)&destSin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler (Ziel)");
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* destSession = libssh2_session_init();
    if (!destSession) {
        LOG_ERROR(L"Fehler beim Erstellen der Ziel-SSH Session");
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    applySshConnectTimeout(destSession);
    
    if (libssh2_session_handshake(destSession, destSock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler (Ziel)");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(destSession, destAccount.host())) {
        libssh2_session_disconnect(destSession, "Normal Shutdown");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(destSession, destAccount.user().c_str(), destAccount.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler (Ziel)");
        libssh2_session_disconnect(destSession, "Normal Shutdown");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP* destSftpSession = libssh2_sftp_init(destSession);
    if (!destSftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der Ziel-SFTP Session");
        libssh2_session_disconnect(destSession, "Normal Shutdown");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP_HANDLE* srcFile = libssh2_sftp_open(srcSftpSession, srcRemotePath.c_str(),
        LIBSSH2_FXF_READ, 0);
    if (!srcFile) {
        LOG_ERROR(L"Fehler beim Öffnen der Quell-Datei: " + string_to_wstring(srcRemotePath));
        libssh2_sftp_shutdown(destSftpSession);
        libssh2_session_disconnect(destSession, "Normal Shutdown");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP_HANDLE* destFile = libssh2_sftp_open(destSftpSession, destRemotePath.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    if (!destFile) {
        LOG_ERROR(L"Fehler beim Öffnen der Ziel-Datei: " + string_to_wstring(destRemotePath));
        libssh2_sftp_close(srcFile);
        libssh2_sftp_shutdown(destSftpSession);
        libssh2_session_disconnect(destSession, "Normal Shutdown");
        libssh2_session_free(destSession);
        closesocket(destSock);
        libssh2_sftp_shutdown(srcSftpSession);
        libssh2_session_disconnect(srcSession, "Normal Shutdown");
        libssh2_session_free(srcSession);
        closesocket(srcSock);
        return CopyResult::Error;
    }
    
    // Datei kopieren
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    ssize_t bytesRead;
    CopyResult result = CopyResult::OK;
    
    while ((bytesRead = libssh2_sftp_read(srcFile, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        if (!sftpWriteAll(destFile, buffer.data(), static_cast<size_t>(bytesRead))) {
            LOG_ERROR(L"Fehler beim Schreiben der Ziel-Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytesRead < 0) {
        LOG_ERROR(L"Fehler beim Lesen der Quell-Datei");
        result = CopyResult::Error;
    }
    
    libssh2_sftp_close(destFile);
    libssh2_sftp_close(srcFile);
    libssh2_sftp_shutdown(destSftpSession);
    libssh2_session_disconnect(destSession, "Normal Shutdown");
    libssh2_session_free(destSession);
    closesocket(destSock);
    libssh2_sftp_shutdown(srcSftpSession);
    libssh2_session_disconnect(srcSession, "Normal Shutdown");
    libssh2_session_free(srcSession);
    closesocket(srcSock);
    
    return result;
}
#endif
#endif

#ifdef _UNIX
CopyResult copyFsToSsh(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copyFsToSsh: " + src + L" -> " + dest);
    
    // SSH-Account aus dem Zielpfad extrahieren
    auto accountOpt = getAccountFromPath(dest);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + dest);
        return CopyResult::Error;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(dest);
    
    // Lokale Datei zum Lesen öffnen
    std::string srcPath = wstring_to_string(src);
    std::ifstream localFile(srcPath, std::ios::binary);
    if (!localFile.is_open()) {
        LOG_ERROR(L"Fehler beim Öffnen der lokalen Datei: " + src);
        return CopyResult::Error;
    }
    
    // SSH-Session erstellen
    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return CopyResult::Error;
    }
    
    // Optionen setzen
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);
    
    // Verbinden
    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // Authentifizierung
    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // SFTP Session
    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }

    // Remote-Datei zum Schreiben öffnen
    sftp_file remoteFile = sftp_open(sftpSession, remotePath.c_str(), 
                                      O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (remoteFile == nullptr) {
        LOG_ERROR(L"Fehler beim Öffnen der Remote-Datei zum Schreiben: " + string_to_wstring(remotePath));
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return CopyResult::Error;
    }
    
    // Datei kopieren
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    CopyResult result = CopyResult::OK;
    
    while (localFile.read(buffer.data(), buffer.size()) || localFile.gcount() > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        
        std::streamsize bytesRead = localFile.gcount();
        if (!sftpWriteAll(remoteFile, buffer.data(), static_cast<size_t>(bytesRead))) {
            LOG_ERROR(L"Fehler beim Schreiben der Remote-Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (localFile.bad()) {
        LOG_ERROR(L"Fehler beim Lesen der lokalen Datei");
        result = CopyResult::Error;
    }
    
    // Aufräumen
    localFile.close();
    sftp_close(remoteFile);
    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);
    
    return result;
}
#else
#ifdef _WIN32
CopyResult copyFsToSsh(const wstring& src, const wstring& dest)
{
    LOG_DEBUG(L"copyFsToSsh: " + src + L" -> " + dest);
    
    auto accountOpt = getAccountFromPath(dest);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + dest);
        return CopyResult::Error;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(dest);
    
    std::string srcPath = wstring_to_string(src);
    std::ifstream localFile(srcPath, std::ios::binary);
    if (!localFile.is_open()) {
        LOG_ERROR(L"Fehler beim Öffnen der lokalen Datei: " + src);
        return CopyResult::Error;
    }

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return CopyResult::Error;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return CopyResult::Error;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return CopyResult::Error;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP_HANDLE* remoteFile = libssh2_sftp_open(sftpSession, remotePath.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR);
    if (!remoteFile) {
        LOG_ERROR(L"Fehler beim Öffnen der Remote-Datei zum Schreiben: " + string_to_wstring(remotePath));
        libssh2_sftp_shutdown(sftpSession);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    // Datei kopieren
    std::vector<char> buffer(65536); // 64KB Puffer für bessere Kompatibilität
    CopyResult result = CopyResult::OK;
    
    while (localFile.read(buffer.data(), buffer.size()) || localFile.gcount() > 0) {
        if (Synchronize::stopRequested()) {
            LOG_DEBUG(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        std::streamsize bytesRead = localFile.gcount();
        if (!sftpWriteAll(remoteFile, buffer.data(), static_cast<size_t>(bytesRead))) {
            LOG_ERROR(L"Fehler beim Schreiben der Remote-Datei");
            result = CopyResult::Error;
            break;
        }
    }
    
    if (result == CopyResult::OK && localFile.bad()) {
        LOG_ERROR(L"Fehler beim Lesen der lokalen Datei");
        result = CopyResult::Error;
    }
    
    // Aufräumen
    localFile.close();
    libssh2_sftp_close(remoteFile);
    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return result;
}
#endif
#endif

#ifdef _UNIX
bool sshExists(const std::wstring& path)
{
    LOG_DEBUG(L"sshExists: " + path);
    
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);
    
    // SSH-Session erstellen
    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return false;
    }
    
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);
    
    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return false;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    // Prüfen ob Pfad existiert
    sftp_attributes attr = sftp_stat(sftpSession, remotePath.c_str());
    bool exists = (attr != nullptr);
    if (attr) {
        sftp_attributes_free(attr);
    }
    
    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);

    return exists;
}

bool sshIsDirectory(const std::wstring& path)
{
    LOG_DEBUG(L"sshIsDirectory: " + path);
#ifdef _TEST
    ++sshIsDirectoryCallCount;
#endif

    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);

    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return false;
    }

    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);

    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return false;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    sftp_attributes attr = sftp_stat(sftpSession, remotePath.c_str());
    bool isDir = attr != nullptr && attr->type == SSH_FILEXFER_TYPE_DIRECTORY;
    if (attr) {
        sftp_attributes_free(attr);
    }

    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);

    return isDir;
}
#else
#ifdef _WIN32
bool sshExists(const std::wstring& path)
{
    LOG_DEBUG(L"sshExists: " + path);

    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return false;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return false;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftpSession, remotePath.c_str(), &attrs);
    bool exists = (rc == 0);

    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return exists;
}

bool sshIsDirectory(const std::wstring& path)
{
    LOG_DEBUG(L"sshIsDirectory: " + path);
#ifdef _TEST
    ++sshIsDirectoryCallCount;
#endif

    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return false;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return false;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftpSession, remotePath.c_str(), &attrs);
    bool isDir = rc == 0 && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return isDir;
}
#endif
#endif

#ifdef _UNIX
bool sshMkdir(const std::wstring& path)
{
    LOG_DEBUG(L"sshMkdir: " + path);
    
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);
    
    // SSH-Session erstellen
    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return false;
    }
    
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);
    
    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return false;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    // Verzeichnis erstellen mit Berechtigungen 755
    rc = sftp_mkdir(sftpSession, remotePath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    bool success = (rc == SSH_OK);
    if (!success) {
        LOG_ERROR(L"Fehler beim Erstellen des Remote-Verzeichnisses: " + string_to_wstring(remotePath));
    }
    
    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);
    
    return success;
}
#else
#ifdef _WIN32
bool sshMkdir(const std::wstring& path)
{
    LOG_DEBUG(L"sshMkdir: " + path);
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = win_to_sftp_path(getRemotePath(path));

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return false;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return false;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    int rc = libssh2_sftp_mkdir(sftpSession, remotePath.c_str(),
        LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP | LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
    bool success = (rc == 0);
    if (!success) {
        unsigned long e = libssh2_sftp_last_error(sftpSession);
        LOG_ERROR(L"Fehler beim Erstellen des Remote-Verzeichnisses: " + string_to_wstring(remotePath));
    }
    
    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return success;
}
#endif
#endif

#ifdef _UNIX
bool sshRemoveFile(const std::wstring& path)
{
    LOG_DEBUG(L"sshRemoveFile: " + path);

    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);

    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return false;
    }

    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);

    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return false;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return false;
    }

    rc = sftp_unlink(sftpSession, remotePath.c_str());
    bool success = (rc == SSH_OK);
    if (!success) {
        LOG_ERROR(L"Fehler beim Löschen der Remote-Datei: " + string_to_wstring(remotePath));
    }

    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);

    return success;
}
#else
#ifdef _WIN32
bool sshRemoveFile(const std::wstring& path)
{
    LOG_DEBUG(L"sshRemoveFile: " + path);
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return false;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = win_to_sftp_path(getRemotePath(path));

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return false;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return false;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    int rc = libssh2_sftp_unlink(sftpSession, remotePath.c_str());
    bool success = (rc == 0);
    if (!success) {
        LOG_ERROR(L"Fehler beim Löschen der Remote-Datei: " + string_to_wstring(remotePath));
    }

    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return success;
}
#endif
#endif

#ifdef _UNIX
std::vector<SshDirEntry> sshListDir(const std::wstring& path)
{
    LOG_DEBUG(L"sshListDir: " + path);
    std::vector<SshDirEntry> entries;
    
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return entries;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);
    
    ssh_session sshSession = ssh_new();
    if (sshSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        return entries;
    }
    
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_USER, account.user().c_str());
    applySshConnectTimeout(sshSession);
    
    int rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SSH Verbindungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_free(sshSession);
        return entries;
    }

    if (!verifySshHostKey(sshSession, account.host())) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return entries;
    }

    rc = ssh_userauth_password(sshSession, nullptr, account.password().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        LOG_ERROR(L"SSH Authentifizierungsfehler: " + string_to_wstring(ssh_get_error(sshSession)));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return entries;
    }

    sftp_session sftpSession = sftp_new(sshSession);
    if (sftpSession == nullptr) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return entries;
    }
    
    rc = sftp_init(sftpSession);
    if (rc != SSH_OK) {
        LOG_ERROR(L"SFTP Initialisierungsfehler");
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return entries;
    }
    
    // Verzeichnis öffnen
    sftp_dir dir = sftp_opendir(sftpSession, remotePath.c_str());
    if (dir == nullptr) {
        LOG_ERROR(L"Fehler beim Öffnen des Remote-Verzeichnisses: " + string_to_wstring(remotePath));
        sftp_free(sftpSession);
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        return entries;
    }
    
    // Verzeichniseinträge lesen
    sftp_attributes attr;
    while ((attr = sftp_readdir(sftpSession, dir)) != nullptr) {
        std::string name = attr->name;
        // "." und ".." überspringen
        if (name != "." && name != "..") {
            SshDirEntry entry;
            entry.name = string_to_wstring(name);
            entry.isDirectory = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
            entry.isRegularFile = (attr->type == SSH_FILEXFER_TYPE_REGULAR);
            entry.isSymlink = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
            entry.size = attr->size;
            entries.push_back(entry);
        }
        sftp_attributes_free(attr);
    }
    // sftp_readdir returns NULL for both EOF and error - only sftp_dir_eof
    // tells them apart. A connection dropped mid-listing must not pass off
    // the partial listing silently as the complete directory.
    if (!sftp_dir_eof(dir))
        LOG_ERROR(L"Remote-Verzeichnis unvollständig gelesen: " + string_to_wstring(remotePath) + L" - " + string_to_wstring(ssh_get_error(sshSession)));

    sftp_closedir(dir);
    sftp_free(sftpSession);
    ssh_disconnect(sshSession);
    ssh_free(sshSession);
    
    return entries;
}
#else
#ifdef _WIN32
std::vector<SshDirEntry> sshListDir(const std::wstring& path)
{
    LOG_DEBUG(L"sshListDir: " + path);
    std::vector<SshDirEntry> entries;
    
    auto accountOpt = getAccountFromPath(path);
    if (!accountOpt.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden für: " + path);
        return entries;
    }
    const SshAccount& account = accountOpt.value();
    std::string remotePath = getRemotePath(path);

    ensureLibssh2Initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR(L"Fehler beim Erstellen des Sockets");
        return entries;
    }

    sockaddr_in sin{};
    if (!resolveHost(account.host(), sin)) {
        LOG_ERROR(L"Fehler beim Auflösen des Hostnamens: " + string_to_wstring(account.host()));
        closesocket(sock);
        return entries;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR(L"SSH Verbindungsfehler");
        closesocket(sock);
        return entries;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        LOG_ERROR(L"Fehler beim Erstellen der SSH Session");
        closesocket(sock);
        return entries;
    }
    applySshConnectTimeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        LOG_ERROR(L"SSH Handshake-Fehler");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    if (!verifySshHostKey(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        LOG_ERROR(L"SSH Authentifizierungsfehler");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    LIBSSH2_SFTP* sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        LOG_ERROR(L"Fehler beim Erstellen der SFTP Session");
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }
    
    LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(sftpSession, remotePath.c_str());
    if (!dir) {
        LOG_ERROR(L"Fehler beim Öffnen des Remote-Verzeichnisses: " + string_to_wstring(remotePath));
        libssh2_sftp_shutdown(sftpSession);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }
    
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    while ((rc = libssh2_sftp_readdir(dir, buffer, sizeof(buffer), &attrs)) > 0) {
        std::string name = buffer;
        if (name != "." && name != "..") {
            SshDirEntry entry;
            entry.name = string_to_wstring(name);
            entry.isDirectory = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
            entry.isRegularFile = LIBSSH2_SFTP_S_ISREG(attrs.permissions);
            entry.isSymlink = LIBSSH2_SFTP_S_ISLNK(attrs.permissions);
            entry.size = attrs.filesize;
            entries.push_back(entry);
        }
    }
    // 0 is EOF, negative is an error mid-listing - see the UNIX variant above.
    if (rc < 0)
        LOG_ERROR(L"Remote-Verzeichnis unvollständig gelesen: " + string_to_wstring(remotePath));

    libssh2_sftp_closedir(dir);
    libssh2_sftp_shutdown(sftpSession);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return entries;
}
#endif
#endif
