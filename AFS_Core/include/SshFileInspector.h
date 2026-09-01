#pragma once

#include <string>

#ifdef _UNIX
    #include <libssh/libssh.h>
    #include <libssh/sftp.h>
#else
    // Windows: libssh2 als Alternative
    #ifdef _WIN32
        #include <libssh2.h>
        #include <libssh2_sftp.h>
    #endif
#endif

#include "SshAccount.h"

// Struktur für die Rückgabewerte
struct RemoteFileInfo {
    bool exists;
    uint64_t size_bytes;
    std::string last_modified_iso; // Lesbarer Zeitstempel
    time_t raw_mtime;              // Für programmatische Vergleiche
};

class SshFileInspector {
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
  // Verbindet, prueft den Host-Key und meldet sich an; wirft, wenn davon etwas
  // scheitert. Ein erfolgreich erzeugtes Objekt heisst: die Gegenstelle steht.
  SshFileInspector(const SshAccount& account);
  ~SshFileInspector();
  // remote_path ist der Pfad auf der Gegenstelle **ohne** Kontopraefix, also
  // "/home/joachim/daten" und nicht "pisi:/home/joachim/daten". Die Umschrift
  // fuer Windows-Pfade ("C:\dir" -> "/C:/dir") macht die Funktion selbst.
  // exists=false bedeutet "verbunden und sicher nicht vorhanden"; jeder andere
  // Fehler wirft, damit ein Verbindungsproblem nicht wie eine geloeschte Datei
  // aussieht.
  RemoteFileInfo inspectFile(const std::string& remote_path);
};
