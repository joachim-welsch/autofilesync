#include "pch.h"
#include "SyncTree.h"
#include "Global.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#ifdef _WXWIDGETS
    #include "winutil.h"
#endif
#include "util.h"
#include "Synchronize.h"
#include "Logging.h"
#include "BackupProof.h"
#include <filesystem>
#include <iostream>


using std::wstring;
using namespace util;
namespace fs = std::filesystem;
using std::exception;

SyncTree::SyncTree(const SyncItem& si, std::size_t& copyCount, bool onlyCount)
    : SyncBase(si, copyCount, onlyCount)
{}

void SyncTree::count(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    LOG_DEBUG(L"count: " + sourcename + L" -> " + targetname);
    if (syncItem_.sshSource()) {
        syncSshBranch(sourcename, targetname, wildcard);
    } else {
        syncBranch(withoutVolumeSpec(sourcename), withoutVolumeSpec(targetname), wildcard);
    }
}

void SyncTree::syncSshDirEntry(const SshDirEntry& entry, const wstring& sourcePath, const wstring& wildcard, const wstring& destination)
{
    LOG_DEBUG(L"syncSshDirEntry: " + entry.name);
    
    // Nur reguläre Dateien verarbeiten (Verzeichnisse werden in syncSshBranch behandelt)
    if (!entry.isRegularFile) {
        return;
    }
    
    // Wildcard-Matching prüfen
    if (!wildcard.empty() && !wildmatch(entry.name, wildcard)) {
        return;
    }
    if (isExcluded(entry.name, syncItem_.exclusions())) {
        return;
    }

    if (onlyCount_) {
        copyCount_++;
        return;
    }

    wstring fname = util::withTrailingChar(sourcePath, L'/') + entry.name;
    wstring dest = util::withTrailingChar(destination, L'/') + entry.name;
    
    SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), fname, dest, wildcard, Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
}

void SyncTree::syncSshBranch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    LOG_DEBUG(L"syncSshBranch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopRequested())
        return;
    // Names seen at this directory level's source listing, so the
    // target-only pass below can tell "not in source" from "wildcard-
    // excluded, ignore" - see SyncBase::syncTargetOnlyEntries(). Includes
    // subdirectory names too, so a target file that happens to share a
    // source subdirectory's name is never mistaken for an orphan.
    std::set<wstring> sourceNames;
    try {
        createTargetDirIfNotExist(targetname);

        auto entries = sshListDir(sourcename);
        for (const auto& entry : entries) {
            if (entry.name == L"." || entry.name == L"..") {
                continue;
            }
            sourceNames.insert(entry.name);

            // Never follow SSH symlinks - see syncBranch()'s local-path check
            // above for the same reasoning (a link back to an ancestor would
            // recurse forever). Unlike the local path, this can't safely
            // distinguish "symlink to a directory" from "symlink to a file"
            // without an extra round trip per entry, so every symlink is
            // skipped rather than synced - conservative, but correct either way.
            if (entry.isSymlink) {
                LOG_INFO2(L"SSH-Symlink wird nicht verfolgt: ", withTrailingChar(sourcename, L'/') + entry.name)
            } else if (entry.isDirectory) {
                // Ausgeschlossene Verzeichnisse werden komplett übersprungen,
                // nicht erst pro enthaltener Datei gefiltert - eine ganze
                // Teilstruktur (z.B. "node_modules") soll nie betreten werden.
                if (isExcluded(entry.name, syncItem_.exclusions())) {
                    LOG_DEBUG(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + entry.name)
                } else {
                    // Rekursiv in Unterverzeichnis absteigen
                    wstring source = withTrailingChar(sourcename, L'/') + entry.name;
                    wstring target = withTrailingChar(targetname, L'/') + entry.name;
                    syncSshBranch(source, target, wildcard);
                }
            } else {
                // Datei synchronisieren
                syncSshDirEntry(entry, sourcename, wildcard, targetname);
            }

            if (Synchronize::stopRequested())
                break;
        }
        // Target-only subdirectories are intentionally out of scope here (see
        // syncTargetOnlyEntries()'s doc comment) - only files at this level.
        if (!Synchronize::stopRequested())
            syncTargetOnlyEntries(sourcename, targetname, wildcard, sourceNames);
    }
    catch (const std::exception& e) {
        LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
        reportSyncError(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        LOG_ERROR3(sourcename, targetname, L" - Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler: " + sourcename);
    }
}

void SyncTree::syncBranch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    LOG_DEBUG(L"syncBranch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopRequested())
        return;
    SyncItem syncItem(L"", SyncItemType::Dir, sourcename, targetname, wildcard, syncItem_.exclusions(), syncItem_.flags(),
            syncItem_.compareItems());
    wstring search = withTrailingChar(sourcename, DIR_SEPARATOR);
    std::set<wstring> sourceNames;
    try {
        createTargetDirIfNotExist(targetname);
        for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(search)) {
            const auto& newpath = dir_entry.path();
            if (newpath.has_filename()) {
                wstring filename;
                try {
                    filename = newpath.filename().wstring();
                } catch (const exception& e) {
                    LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
                    continue;
                }
                if (filename != L"." && filename != L"..")
                    sourceNames.insert(filename);
                if (dir_entry.is_directory() && filename != L"." && filename != L"..") {
                    // Never follow directory symlinks: a link back to an ancestor
                    // (or any other cycle) would otherwise make this recurse
                    // forever, exhausting the stack.
                    if (dir_entry.is_symlink()) {
                        LOG_INFO2(L"Symlink-Verzeichnis wird nicht verfolgt: ", withTrailingChar(sourcename, DIR_SEPARATOR) + filename)
                    } else if (isExcluded(filename, syncItem_.exclusions())) {
                        // Ganze Teilstruktur überspringen, nicht erst pro Datei filtern.
                        LOG_DEBUG(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + filename)
                    } else {
                        wstring target = withTrailingChar(targetname, DIR_SEPARATOR) + filename;
                        wstring source = withTrailingChar(sourcename, DIR_SEPARATOR) + filename;
                        syncBranch(source, target, wildcard);
                    }
                } else
                    syncDirEntry(dir_entry, wildcard, targetname);
            }
            if (Synchronize::stopRequested())
                break;
        }
        if (!Synchronize::stopRequested())
            syncTargetOnlyEntries(sourcename, targetname, wildcard, sourceNames);
    }
    catch (const std::exception& e) {
        LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
        reportSyncError(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        LOG_ERROR3(sourcename, targetname, L" - Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler: " + sourcename);
    }
}

void SyncTree::syncLocalToSshBranch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    LOG_DEBUG(L"syncLocalToSshBranch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopRequested())
        return;

    wstring search = withTrailingChar(sourcename, DIR_SEPARATOR);
    std::set<wstring> sourceNames;
    try {
        createTargetDirIfNotExist(targetname);
        for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(search)) {
            const auto& newpath = dir_entry.path();
            if (newpath.has_filename()) {
                wstring filename;
                try {
                    filename = newpath.filename().wstring();
                } catch (const exception& e) {
                    LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
                    continue;
                }
                if (filename != L"." && filename != L"..")
                    sourceNames.insert(filename);
                if (dir_entry.is_directory() && filename != L"." && filename != L"..") {
                    // Never follow directory symlinks - see syncBranch() above.
                    if (dir_entry.is_symlink()) {
                        LOG_INFO2(L"Symlink-Verzeichnis wird nicht verfolgt: ", withTrailingChar(sourcename, DIR_SEPARATOR) + filename)
                    } else if (isExcluded(filename, syncItem_.exclusions())) {
                        LOG_DEBUG(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + filename)
                    } else {
                        // SSH-Ziel: POSIX '/' verwenden
                        wstring target = withTrailingChar(targetname, L'/') + filename;
                        wstring source = withTrailingChar(sourcename, DIR_SEPARATOR) + filename;
                        syncLocalToSshBranch(source, target, wildcard);
                    }
                } else if (dir_entry.is_regular_file()) {
                    // Wildcard-Matching prüfen
                    if (!wildcard.empty() && !wildmatch(filename, wildcard)) {
                        continue;
                    }
                    if (isExcluded(filename, syncItem_.exclusions())) {
                        continue;
                    }

                    if (onlyCount_) {
                        copyCount_++;
                    } else {
                        wstring source = withTrailingChar(sourcename, DIR_SEPARATOR) + filename;
                        wstring dest = withTrailingChar(targetname, L'/') + filename;
                        SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), source, dest, wildcard,
                                 Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
                    }
                }
            }
            if (Synchronize::stopRequested())
                break;
        }
        if (!Synchronize::stopRequested())
            syncTargetOnlyEntries(sourcename, targetname, wildcard, sourceNames);
    }
    catch (const std::exception& e) {
        LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
        reportSyncError(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        LOG_ERROR3(sourcename, targetname, L" - Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler: " + sourcename);
    }
}

void SyncTree::syncSshToSshBranch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    LOG_DEBUG(L"syncSshToSshBranch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopRequested())
        return;

    std::set<wstring> sourceNames;
    try {
        createTargetDirIfNotExist(targetname);

        auto entries = sshListDir(sourcename);
        for (const auto& entry : entries) {
            if (entry.name == L"." || entry.name == L"..") {
                continue;
            }
            sourceNames.insert(entry.name);

            // Never follow SSH symlinks - see syncSshBranch() above.
            if (entry.isSymlink) {
                LOG_INFO2(L"SSH-Symlink wird nicht verfolgt: ", withTrailingChar(sourcename, L'/') + entry.name)
            } else if (entry.isDirectory) {
                if (isExcluded(entry.name, syncItem_.exclusions())) {
                    LOG_DEBUG(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + entry.name)
                } else {
                    // Rekursiv in Unterverzeichnis absteigen (beide SSH-Pfade: POSIX '/')
                    wstring source = withTrailingChar(sourcename, L'/') + entry.name;
                    wstring target = withTrailingChar(targetname, L'/') + entry.name;
                    syncSshToSshBranch(source, target, wildcard);
                }
            } else if (entry.isRegularFile) {
                // Wildcard-Matching prüfen
                if (!wildcard.empty() && !wildmatch(entry.name, wildcard)) {
                    continue;
                }
                if (isExcluded(entry.name, syncItem_.exclusions())) {
                    continue;
                }

                if (onlyCount_) {
                    copyCount_++;
                } else {
                    wstring source = withTrailingChar(sourcename, L'/') + entry.name;
                    wstring dest = withTrailingChar(targetname, L'/') + entry.name;
                    SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), source, dest, wildcard,
                             Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
                }
            }

            if (Synchronize::stopRequested())
                break;
        }
        if (!Synchronize::stopRequested())
            syncTargetOnlyEntries(sourcename, targetname, wildcard, sourceNames);
    }
    catch (const std::exception& e) {
        LOG_ERROR3(sourcename, targetname, string_to_wstring(e.what()))
        reportSyncError(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        LOG_ERROR3(sourcename, targetname, L" - Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler: " + sourcename);
    }
}

void SyncTree::operator()()
{
	LOG_DEBUG(L"SyncTree::operator()()");

	if (syncItem_.sshSource() && syncItem_.sshTarget()) {
		// SSH-zu-SSH: Beide Endpunkte sind SSH-Pfade
		LOG_DEBUG(L"SyncTree SSH-zu-SSH: " + syncItem_.source() + L" -> " + syncItem_.target());
		syncSshToSshBranch(syncItem_.source(), syncItem_.target(), syncItem_.wildcard());
	}
	else if (syncItem_.sshSource()) {
		// SSH-Quelle: Verwende SSH-basiertes Directory Listing
		LOG_DEBUG(L"SyncTree mit SSH-Quelle: " + syncItem_.source());
		syncSshBranch(syncItem_.source(), 
					  syncItem_.sshTarget() ? syncItem_.target() : withoutVolumeSpec(syncItem_.target()), 
					  syncItem_.wildcard());
	}
	else if (syncItem_.sshTarget()) {
		// Lokale Quelle, SSH-Ziel
		LOG_DEBUG(L"SyncTree lokal->SSH: " + syncItem_.source() + L" -> " + syncItem_.target());
		syncLocalToSshBranch(withoutVolumeSpec(syncItem_.source()), syncItem_.target(), syncItem_.wildcard());
	}
	else {
		// Lokale Quelle
		syncBranch(withoutVolumeSpec(syncItem_.source()), 
				   syncItem_.sshTarget() ? syncItem_.target() : withoutVolumeSpec(syncItem_.target()), 
				   syncItem_.wildcard());
	}
}
