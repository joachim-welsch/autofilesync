#pragma once

#include <string>
#include <optional>
#include <vector>
#include <atomic>
#include "SshAccount.h"
#include "SyncBase.h"  // für CopyResult

namespace AFS_Core {

// Struktur für SSH-Verzeichniseinträge
struct SshDirEntry {
    std::wstring name;
    bool isDirectory = false;
    bool isRegularFile = false;
    bool isSymlink = false;
    uint64_t size = 0;
};

// Hilfsfunktionen für SSH-Pfade
std::optional<SshAccount> getAccountFromPath(const std::wstring& path);
std::string getRemotePath(const std::wstring& path);
std::string win_to_sftp_path(const std::string& win);

// SSH-Kopierfunktionen
CopyResult copySshToFs(const std::wstring& src, const std::wstring& dest);
CopyResult copySshToSsh(const std::wstring& src, const std::wstring& dest);
CopyResult copyFsToSsh(const std::wstring& src, const std::wstring& dest);

// SSH-Verzeichnisfunktionen
bool sshExists(const std::wstring& path);
bool sshIsDirectory(const std::wstring& path);
bool sshMkdir(const std::wstring& path);
bool sshRemoveFile(const std::wstring& path);
std::vector<SshDirEntry> sshListDir(const std::wstring& path);

#ifdef _TEST
// Counts real SSH directory checks (each one is a full connect), so tests can
// verify Synchronize's per-target cache actually avoids reconnecting.
extern std::atomic<int> sshIsDirectoryCallCount;
#endif

} // namespace AFS_Core
