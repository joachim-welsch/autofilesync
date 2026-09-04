#include "pch.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "util.h"
#include "Logging.h"
#include "Global.h"
#include "BackupProof.h"
#include <iostream>

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncDir::SyncDir(const SyncItem& si, std::size_t& copyCount, bool onlyCount)
    : SyncBase(si, copyCount, onlyCount)
{}

void SyncDir::syncSshDirEntry(const SshDirEntry& entry, const wstring& sourcePath, const wstring& wildcard, const wstring& destination)
{
    LOG_DEBUG(L"syncSshDirEntry: " + entry.name);
    
    // Nur reguläre Dateien verarbeiten (keine Verzeichnisse bei SyncDir)
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

    // sourcePath is always SSH here (this method only runs for an SSH source);
    // destination may still be a local path, so only it depends on sshTarget().
    wstring fname = util::withTrailingChar(sourcePath, L'/') + entry.name;
    wstring dest = util::withTrailingChar(destination, syncItem_.sshTarget() ? L'/' : DIR_SEPARATOR) + entry.name;
    
    SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), fname, dest, wildcard, Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
}

void SyncDir::operator()()
{
    LOG_DEBUG(L"SyncDir::operator()()");
    wstring source = syncItem_.source();
    if (source.empty())
        return;
    wstring destination = syncItem_.target();
    const wstring wildcard = syncItem_.wildcard();
    
    // Names seen in the source, so the target-only pass below can tell "not
    // in source" from "wildcard-excluded, ignore" - see syncTargetOnlyEntries().
    std::set<wstring> sourceNames;
    try {
        createTargetDirIfNotExist();

        // SSH-Quelle behandeln
        if (syncItem_.sshSource()) {
            LOG_DEBUG(L"SyncDir mit SSH-Quelle: " + source);
            auto entries = sshListDir(source);
            for (const auto& entry : entries) {
                sourceNames.insert(entry.name);
                syncSshDirEntry(entry, source, wildcard, destination);
                if (stopRequested())
                    break;
            }
        } else {
            // Lokale Quelle
            wstring localSource = withoutVolumeSpec(source);
            wstring localDest = syncItem_.sshTarget() ? destination : withoutVolumeSpec(destination);
            for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(localSource)) {
                if (dir_entry.path().has_filename())
                    sourceNames.insert(dir_entry.path().filename().wstring());
                syncDirEntry(dir_entry, wildcard, localDest);
                if (stopRequested())
                    break;
            }
        }
        if (!stopRequested())
            syncTargetOnlyEntries(source, destination, wildcard, sourceNames);
    }
    catch (const std::exception& e) {
        LOG_ERROR(string_to_wstring(e.what()));
        reportSyncError(string_to_wstring(e.what()));
    }
}
