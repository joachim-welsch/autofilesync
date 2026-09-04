#include "pch.h"
#include "CheckPath.h"
#include <sstream>
#include "Logging.h"
#include "util.h"
#include "MountedDrives.h"
#include "Config.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include <FilesystemUtil.h>
#include <filesystem>
#include <optional>

using namespace util;
using std::wstring;
namespace fs = std::filesystem;

namespace {
    constexpr uint16_t kBaseInterval = 2;
    // Erreichbar: selten nachpruefen ist billig und aendert nichts am Ergebnis.
    constexpr uint16_t kMaxIntervalReachable = 3600;
    // Nicht erreichbar: hier muss die Pruefung zeitnah wiederkommen. Mit einer
    // Stunde Backoff bleibt der Sync nach einem spaeten Mount (Boot-Race) oder
    // einem Netzwechsel noch lange blind, obwohl das Ziel laengst wieder da ist.
    constexpr uint16_t kMaxIntervalUnreachable = 300;

    // Ein Pfad gehoert zu einer SSH-Verbindung, wenn vor dem ersten Doppelpunkt
    // der Name eines konfigurierten Kontos steht. Ein Laufwerksbuchstabe
    // ("C:\...") sieht aehnlich aus, findet aber kein Konto.
    std::optional<SshAccount> sshAccountOfPath(const wstring& path, wstring& remotePath)
    {
        const auto colonPos = path.find(L':');
        if (colonPos == wstring::npos || colonPos == 0)
            return std::nullopt;
        const wstring accountName = path.substr(0, colonPos);
        auto account = Config::instance().findSshAccountByName(wstring_to_string(accountName));
        if (!account.has_value())
            return std::nullopt;
        remotePath = path.substr(colonPos + 1);
        return account;
    }
}

bool pathContainsVolumeName(const wstring& path)
{
	LOG_DEBUG(L"pathContainsVolumeName: " + path);
	auto openParenthesisPos = path.find(L"(");
	auto colonPos = path.find(L":");
    bool retval = openParenthesisPos != wstring::npos && colonPos != wstring::npos && openParenthesisPos < colonPos;
    if (retval)
        LOG_DEBUG(L"pathContainsVolumeName returns true")
    else
        LOG_DEBUG(L"pathContainsVolumeName returns false")
    return retval;
}

wstring volumeName(const wstring& path)
{
    LOG_DEBUG(L"volumeName() " + path);
	wstring retval;
    if (pathContainsVolumeName(path)) {
		retval = path.substr(0, path.find(L"(") - 1);
	}
	return retval;
}

uint16_t nextCheckInterval(uint16_t previousInterval, bool repeatsPreviousResult, bool reachable)
{
    if (!repeatsPreviousResult)
        return kBaseInterval;
    const uint16_t maxInterval = reachable ? kMaxIntervalReachable : kMaxIntervalUnreachable;
    const uint32_t doubled = static_cast<uint32_t>(previousInterval) * 2u;
    return doubled > maxInterval ? maxInterval : static_cast<uint16_t>(doubled);
}

bool CheckPath::pathExists(const wstring& pathname, bool mustExist) const
{
    LOG_DEBUG(L"drivesExists()");
#ifdef _UNIX
    (void)mustExist;
    fs::path path(pathname);
    // Wenn es eine Datei ist, nehme das Eltern-Verzeichnis
    if (fs::is_regular_file(path)) {
        path = path.parent_path();
    }
    // Prüfe ob der Pfad existiert (entweder Mountpoint oder regulärer Pfad)
    auto mountResult = fsu::isMountpoint(path);
    if (mountResult.has_value()) {
        // Es ist ein Mountpoint - prüfe ob er gemountet ist
        return fs::exists(path);
    }
    // Kein Mountpoint - prüfe ob der Pfad existiert und zugänglich ist
    return fs::exists(path);
#else
    const wstring driveName = rootName(pathname);
    if (pathContainsVolumeName(pathname) && !volumeExists(driveName, volumeName(pathname)))
        return false;
    // driveExists() only ever saw the drive letter, never the path itself: a
    // source folder deleted underneath an existing drive still counted as
    // reachable, so the pass fell through to SyncDir, whose exception got
    // reported unconditionally on every single pass instead of once (see
    // BackupProofTest.UnreachableSourceIsProvenOnceNotOncePerPass). A target
    // still has to be allowed to not exist yet - SyncDir/SyncTree create it.
    if (!mustExist)
        return ::driveExists(driveName);
    std::error_code ec;
    return fs::exists(withoutVolumeSpec(pathname), ec);
#endif
}

bool CheckPath::sshPathReachable(const wstring& pathname, bool mustExist) const
{
    LOG_DEBUG(L"sshPathReachable: " + pathname);
    wstring remotePath;
    auto account = sshAccountOfPath(pathname, remotePath);
    if (!account.has_value()) {
        // Kein Konto zu diesem Praefix: der Pfad laesst sich nicht aufloesen,
        // also ist auch nichts erreichbar.
        LOG_ERROR(L"SSH-Account nicht gefunden fuer: " + pathname);
        return false;
    }
    try {
        // Der Konstruktor verbindet, prueft den Host-Key und meldet sich an und
        // wirft, wenn davon etwas scheitert. Das ist die Entsprechung zum
        // "Laufwerk ist gemountet" der lokalen Pruefung.
        SshFileInspector inspector(account.value());
        if (!mustExist)
            return true;
        // sftp_stat beantwortet Dateien wie Verzeichnisse. exists=false heisst
        // "verbunden und sicher nicht da" - fuer eine Quelle ist das ein Grund
        // auszusetzen, nicht einer, das Ziel anzufassen: sonst raeumt ein
        // geloeschtes Quellverzeichnis mit eingeschaltetem "Geloeschte Dateien
        // im Ziel ebenfalls loeschen" das Ziel leer.
        const RemoteFileInfo info = inspector.inspectFile(wstring_to_string(remotePath));
        if (!info.exists)
            LOG_INFO(L"SSH-Pfad existiert nicht: " + pathname);
        return info.exists;
    }
    catch (const std::exception& e) {
        LOG_INFO(L"SSH-Pfad nicht erreichbar: " + pathname + L" - " + string_to_wstring(e.what()));
        return false;
    }
    catch (...) {
        LOG_INFO(L"SSH-Pfad nicht erreichbar: " + pathname);
        return false;
    }
}

bool CheckPath::sideReachable(const wstring& pathname, bool mustExist) const
{
    wstring remotePath;
    if (sshAccountOfPath(pathname, remotePath).has_value())
        return sshPathReachable(pathname, mustExist);
    return pathExists(pathname, mustExist);
}

bool CheckPath::operator()(const std::wstring& source, const std::wstring& dest, bool ssh)
{
    (void)ssh; // Welche Seite ueber SSH liegt, entscheidet der Pfad selbst.
        // Cache-Schlüssel aus Quell- und Zielverzeichnis erstellen
    std::wstring cacheKey = source + L"|" + dest;

    std::lock_guard<std::mutex> lock(driveCheckCacheMutex_);

    // Prüfen, ob ein gültiger Cache-Eintrag existiert
    bool havePrevious = false;
    bool previousResult = true;
    uint16_t previousInterval = 2;
    auto it = driveCheckCache_.find(cacheKey);
    LOG_DEBUG(cacheKey);
    if (it != driveCheckCache_.end()) {
        LOG_DEBUG(L"cache key found()");
        const auto& cacheEntry = it->second;
        auto now = std::chrono::steady_clock::now();
        auto cacheAge = std::chrono::duration_cast<std::chrono::seconds>(now - cacheEntry.timestamp).count();
		std::ostringstream debugMsg;
		debugMsg << "Cache age: " << cacheAge << " seconds, validity: " << cacheEntry.timeIntervall_ << " seconds";
		LOG_DEBUG(debugMsg.str());
        if (cacheAge < cacheEntry.timeIntervall_) {
            std::wstringstream sout;
            sout << L"checkDrivesExits() using cached result for " << source << "-" << dest
                << L", cache age: " << cacheAge << L"s, result: " << std::boolalpha << cacheEntry.result;
            LOG_DEBUG(sout.str());
            return cacheEntry.result;
        }
        // Expired: only remember it for the backoff decision below, not touched
        // while still valid above - a plain cache hit must not grow the interval.
        havePrevious = true;
        previousResult = cacheEntry.result;
        previousInterval = cacheEntry.timeIntervall_;
        driveCheckCache_.erase(it);
    }

    // Tatsächliche Laufwerksüberprüfung durchführen
    bool result = true;
    // Die Quelle muss vorhanden sein - von dort wird gelesen. Beim Ziel genuegt
    // es, dass die Gegenstelle antwortet: ein noch nicht existierendes
    // Zielverzeichnis legen SyncDir und SyncTree selbst an, und ein Ziel, das
    // erst entstehen soll, darf die Synchronisation nicht blockieren.
	if (!sideReachable(source, true)) {
		std::wstringstream os;
		os << L"Quellverzeichnis nicht erreichbar: " << source;
		LOG_INFO(os.str());
        result = false;
	}
	if (!sideReachable(dest, false)) {
		std::wstringstream os;
		os << L"Zielverzeichnis nicht erreichbar: " << dest;
		LOG_INFO(os.str());
        result = false;
	}
    // Ergebnis im Cache speichern: das Intervall waechst nur, wenn eine erneute
    // Pruefung denselben Zustand bestaetigt; aendert sich der Zustand (Laufwerk
    // erscheint/verschwindet), wird wieder haeufig geprueft statt bis zu einer
    // Stunde lang am alten Backoff festzuhalten.
    DriveCheckCacheEntry cacheEntry;
    cacheEntry.result = result;
    cacheEntry.timestamp = std::chrono::steady_clock::now();
    cacheEntry.timeIntervall_ = nextCheckInterval(previousInterval, havePrevious && previousResult == result, result);
    driveCheckCache_[cacheKey] = cacheEntry;

    if (Logging::instance().shouldLog(LogLevel::Debug)) {
        std::wstringstream sout;
        sout << L"checkPathsExits() cached new result for " << source << "-" << dest
            << L", result: " << std::boolalpha << result;
        LOG_DEBUG(sout.str());
    }
	return result;
}

wstring rootName(const wstring& path)
{
	LOG_DEBUG(L"rootName: " + path);
	if (pathContainsVolumeName(path)) {
		auto openParenthesisPos = path.find(L"(");
		auto closeParenthesisPos = path.find(L")");
		if (openParenthesisPos != wstring::npos && closeParenthesisPos != wstring::npos)
			return path.substr(openParenthesisPos + 1, (closeParenthesisPos - openParenthesisPos) - 1);
		else
			throw std::runtime_error("Ungültiger Pfad: " + wstring_to_string(path));
	}
	wstring retval;
	fs::path p(path);
	retval = p.root_name().wstring();
    LOG_DEBUG(L"rootName returns: " + retval);
	return retval;
}

std::wstring withoutVolumeSpec(const std::wstring& path)
{
	LOG_DEBUG(L"withoutVolumeSpec: " + path);
	// Strip only a real volume spec ("Label (C:)rest", paren before colon).
	// Merely containing parentheses ("/home/x/Docs (2023)/f") must not strip,
	// or the path degenerates to a wrong/empty one and the item never syncs.
	if (!pathContainsVolumeName(path))
		return path;
	auto closeParenthesisPos = path.find(L")");
	if (closeParenthesisPos == wstring::npos)
		return path;
	return rootName(path) + path.substr(closeParenthesisPos + 1);
}