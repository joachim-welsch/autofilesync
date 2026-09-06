#pragma once

#include <string>
#include <vector>
#ifdef _UNIX
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#else
    #ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
    #endif
#endif
#include "SshAccount.h"

namespace AFS_Core {

// Struktur für Verzeichniseinträge
struct RemoteDirEntry {
    std::string name;
    bool is_directory;
    bool is_regular_file;
    bool is_symlink;
    uint64_t size_bytes;
    std::string last_modified_iso;
    time_t raw_mtime;
    uint32_t permissions;
};

// Struktur für die Rückgabe der Verzeichnisinformationen
struct RemoteDirInfo {
    bool exists;
    bool is_directory;
    std::string path;
    std::vector<RemoteDirEntry> entries;
    std::string error_message;
};

class SshDirInspector {
#ifdef _UNIX
    ssh_session my_ssh_session = nullptr;
    sftp_session sftp_session_ptr = nullptr;
#else
    #ifdef _WIN32
    LIBSSH2_SESSION* my_ssh_session = nullptr;
    LIBSSH2_SFTP* sftp_session_ptr = nullptr;
    SOCKET sock = INVALID_SOCKET;
    #endif
#endif
    SshAccount account_;

    void cleanup();

public:
    SshDirInspector(const SshAccount& account);
    ~SshDirInspector();

    // Lädt alle Verzeichnisinformationen im übergebenen Pfad
    RemoteDirInfo inspectDir(const std::string& remote_path);

    // Lädt Verzeichnisinformationen rekursiv
    RemoteDirInfo inspectDirRecursive(const std::string& remote_path);

    // Ermittelt das Home-Verzeichnis des angemeldeten Benutzers. SFTP kennt
    // keinen eigenen Befehl dafür; "." wird vom Server konventionsgemäß
    // relativ zum Login-Verzeichnis aufgelöst.
    std::string getHomeDir();

private:
    void inspectDirRecursiveImpl(const std::string& remote_path, RemoteDirInfo& result);
};

} // namespace AFS_Core
