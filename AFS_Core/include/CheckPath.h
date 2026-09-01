#pragma once
#include <string>
#include <mutex>
#include <map>
#include <chrono>

class CheckPath {
    mutable std::mutex  driveCheckCacheMutex_;

    struct DriveCheckCacheEntry {
        bool result;
        uint16_t timeIntervall_;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::map<std::wstring, DriveCheckCacheEntry> driveCheckCache_;
    // mustExist only affects the Windows branch: a source must actually be
    // there, but a local target that sync itself still has to create must not
    // be rejected for not existing yet - see sideReachable().
    bool pathExists(const std::wstring& pathname, bool mustExist) const;
    // Erreichbarkeit einer Seite, unabhaengig davon ob sie lokal oder ueber SSH
    // liegt; mustExist gilt nur fuer SSH (lokal entscheidet pathExists).
    bool sideReachable(const std::wstring& pathname, bool mustExist) const;
public:
    CheckPath() = default;
    bool operator()(const std::wstring& source, const std::wstring& dest, bool ssh);
    // Verbindet sich mit der Gegenstelle. mustExist=true verlangt zusaetzlich,
    // dass der Pfad dort vorhanden ist. Oeffentlich, damit die Regel testbar
    // ist, ohne den Cache von operator()() zu durchlaufen.
    bool sshPathReachable(const std::wstring& pathname, bool mustExist) const;
};

// Naechstes Pruefintervall des Caches (Sekunden). Frei stehend, damit sich die
// Backoff-Regel testen laesst, ohne die Wartezeiten real abwarten zu muessen.
uint16_t nextCheckInterval(uint16_t previousInterval, bool repeatsPreviousResult, bool reachable);

bool pathContainsVolumeName(const std::wstring& path);
std::wstring volumeName(const std::wstring& path);
std::wstring rootName(const std::wstring& path);
std::wstring withoutVolumeSpec(const std::wstring& path);