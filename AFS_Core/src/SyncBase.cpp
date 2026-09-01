#include "pch.h"
#include "SyncBase.h"
#include "SyncFileSsh.h"
#include "util.h"
#include <filesystem>
#include "Global.h"
#include "SyncFile.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "Synchronize.h"
#include "BackupProof.h"

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncBase::SyncBase(const SyncItem& syncItem, std::size_t& copyCount, bool onlyCount)
        : syncItem_(syncItem)
        , copyCount_(copyCount)
        , onlyCount_(onlyCount)
{}

bool SyncBase::driveNotExist(const wstring& pathname, bool ssh)
{
    LOG_DEBUG(L"driveNotExist: " + pathname);
        if (ssh)
                return false; // TODO SSH-Laufwerke prüfen
    bool retval = !MountedDrives::instance().driveMounted(pathname);
    if (!retval)
        LOG_DEBUG(L"driveNotExist: TRUE");
    return retval;
}

void SyncBase::createTargetDirIfNotExist(const std::wstring& targetdir)
{
        LOG_DEBUG(L"createTargetDirIfNotExist: " + targetdir);
        wstring destination;
        if (targetdir.empty())
                destination = syncItem_.target();
        else
                destination = targetdir;

        if (onlyCount_)
                return;

        // SSH-Ziel behandeln
        if (syncItem_.sshTarget()) {
                if (!sshExists(destination)) {
                        if (!sshMkdir(destination)) {
                                LOG_ERROR(L"Fehler beim Erstellen des SSH-Verzeichnisses: " + destination);
                                reportSyncError(L"SSH-Zielverzeichnis nicht anlegbar: " + destination);
                        }
                }
        } else {
                // Lokales Ziel
                wstring localDest = withoutVolumeSpec(destination);
                if (!fs::exists(localDest)) {
                        try {
                                fs::create_directory(localDest);
                        }
                        catch (std::filesystem::filesystem_error const& ex) {
                                handleLastError(ex);
                                reportSyncError(string_to_wstring(ex.what()));
                        }
                }
        }
}

void SyncBase::syncDirEntry(const fs::directory_entry& dir_entry, const wstring& wildcard, const wstring& destination)
{
    LOG_DEBUG(L"syncDirEntry: " + dir_entry.path().wstring());
    // SyncDir calls this for every entry unfiltered (SyncTree already filters
    // directories out before calling this at all). Without this check, a
    // subdirectory reached fs::file_size() inside FileDifference, which fails
    // for a directory - misread as "source file not found", a harmless no-op
    // normally, but one that could delete a same-named target *file* if
    // deleteInTarget is enabled for this (non-File) item.
    if (dir_entry.is_directory())
        return;
    const auto& newpath = dir_entry.path();
    if (newpath.has_filename() && (wildcard.empty() || wildmatch(newpath.filename().wstring(), wildcard))
        && !isExcluded(newpath.filename().wstring(), syncItem_.exclusions())) {
        if (onlyCount_) {
            copyCount_++;
            LOG_DEBUG(L"syncDirEntry count only");
            return;
        }
        wstring filename = newpath.filename().wstring();
        wstring fname = newpath.wstring();
        // SSH/SFTP destinations always use '/', regardless of the client platform;
        // only a local destination uses the platform separator.
        wchar_t separator = syncItem_.sshTarget() ? L'/' : DIR_SEPARATOR;
        wstring dest = util::withTrailingChar(destination, separator) + filename;
        try {
            SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), fname, dest, wildcard, Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
        } 
        catch (const std::exception& e) {
            LOG_ERROR3(fname, dest, string_to_wstring(e.what()))
            reportSyncError(fname + L": " + string_to_wstring(e.what()));
        }
        catch (...) {
            LOG_ERROR3(fname, dest, L" - Unbekannter Fehler")
            reportSyncError(L"Unbekannter Fehler: " + fname);
        }
    } else {
            LOG_DEBUG2("newpath.has_filename()", newpath.has_filename());
            LOG_DEBUG2("wildcard.empty()", wildcard.empty());
            LOG_DEBUG2("wildmatch(newpath.filename().wstring(), wildcard)", wildmatch(newpath.filename().wstring(), wildcard));
    }
}

void SyncBase::syncTargetOnlyEntries(const wstring& sourceDir, const wstring& targetDir,
    const wstring& wildcard, const std::set<wstring>& sourceNames)
{
    // Cheap to skip entirely when neither option can act on what this would find.
    if (!syncItem_.deleteInTarget() && !syncItem_.twoWaySync())
        return;

    auto handleEntry = [&](const wstring& name, const wstring& sourcePath, const wstring& targetPath) {
        if (sourceNames.count(name) || (!wildcard.empty() && !wildmatch(name, wildcard)) || isExcluded(name, syncItem_.exclusions()))
            return;
        if (onlyCount_) {
            copyCount_++;
            return;
        }
        try {
            // sourcePath is known not to exist (name absent from sourceNames):
            // SyncFile's own FileDifference-based decision (SourceNotExits ->
            // delete if deleteInTarget, or copy back if twoWaySync) does the
            // rest, exactly as it already does for the source-to-target case.
            SyncFile(SyncItem(syncItem_.name(), syncItem_.syncItemType(), sourcePath, targetPath, wildcard, Exclusions(), syncItem_.flags(), syncItem_.compareItems()), copyCount_)();
        }
        catch (const std::exception& e) {
            LOG_ERROR3(sourcePath, targetPath, string_to_wstring(e.what()))
            reportSyncError(targetPath + L": " + string_to_wstring(e.what()));
        }
        catch (...) {
            LOG_ERROR3(sourcePath, targetPath, L" - Unbekannter Fehler")
            reportSyncError(L"Unbekannter Fehler: " + targetPath);
        }
    };

    const wchar_t sourceSeparator = syncItem_.sshSource() ? L'/' : DIR_SEPARATOR;
    if (syncItem_.sshTarget()) {
        for (const auto& entry : sshListDir(targetDir)) {
            if (!entry.isRegularFile)
                continue;
            wstring targetPath = util::withTrailingChar(targetDir, L'/') + entry.name;
            wstring sourcePath = util::withTrailingChar(sourceDir, sourceSeparator) + entry.name;
            handleEntry(entry.name, sourcePath, targetPath);
            if (stopRequested())
                break;
        }
    } else {
        wstring localTargetDir = withoutVolumeSpec(targetDir);
        if (!fs::exists(localTargetDir))
            return;
        for (const fs::directory_entry& dir_entry : fs::directory_iterator(localTargetDir)) {
            if (!dir_entry.is_regular_file())
                continue;
            wstring name = dir_entry.path().filename().wstring();
            wstring sourcePath = util::withTrailingChar(sourceDir, sourceSeparator) + name;
            handleEntry(name, sourcePath, dir_entry.path().wstring());
            if (stopRequested())
                break;
        }
    }
}

bool SyncBase::stopRequested() const
{
    return Synchronize::stopRequested();
}