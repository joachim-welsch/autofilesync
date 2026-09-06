#include "pch.h"
#include "SyncFile.h"
#include <filesystem>
#include <sstream>
#ifdef _WXWIDGETS
    #include "SyncControler.h"
    #include "ProgressDialog.h"
#elif WIN32
    #include <Windows.h>
#endif
#include "Global.h"
#include "Synchronize.h"
#include "util.h"
#include <FilesystemUtil.h>
#include "FileDifference.h"
#include "Logging.h"
#include "Config.h"
#include "FileDelete.h"
#include "SyncFileSsh.h"
#include "BackupProof.h"

namespace AFS_Core {

namespace fs = std::filesystem;
using std::wstring;
using util::handleLastError;
using util::string_to_wstring;
using util::toString;
using fsu::fileExists;
using util::filesEqual;

namespace {
    const size_t TenMB{ 10 * 1024 * 1024 };
}

#ifdef _TEST
// Counts calls to the free waitForUpdatedFiletime() below, so a regression
// test can catch a caller waiting for the same filetime confirmation twice.
std::atomic<int> waitForUpdatedFiletimeCallCount{0};
#endif

void moveFileToBackup(const wstring& destFilename)
{
    LOG_DEBUG(L"moveFileToBackup: " + destFilename);
    int i = 1;
    wstring newDestFilename;
    do {
        std::wstringstream os;
        os << destFilename << ".bak." << i;
        newDestFilename = os.str();
        ++i;
    } while (fileExists(newDestFilename));
    try {
        fs::rename(destFilename.c_str(), newDestFilename.c_str());
    }
    catch (fs::filesystem_error& err) {
        std::wstringstream os;
        os << "Fehler: " << err.what() << ", beim Umbenennen von " << destFilename << " nach " << newDestFilename;
        throw SyncException(os.str());
    }
}

void transferFilename(const wstring& source, wstring& dest)
{
    LOG_DEBUG(L"transferFilename: " + source + L" -> " + dest);
    dest = util::withTrailingChar(dest, DIR_SEPARATOR);
    fs::path p(source);
    dest += p.filename().wstring();
}

SyncFile::SyncFile(const SyncItem& si, std::size_t& copyCount, bool onlyCount, bool transferFilename)
    : SyncBase(si, copyCount, onlyCount)
    , transferFilename_(transferFilename)
{
    setSsh();
}

void SyncFile::setSsh()
{
    ssh_ = SSHPATH::none;
    if (syncItem_.sshSource()) {
        ssh_ = SSHPATH::source;
    } 
    if (syncItem_.sshTarget()) {
        if (ssh_ == SSHPATH::source) {
            ssh_ = SSHPATH::both;
        } else {
            ssh_ = SSHPATH::dest;
        }
    }
}

void waitForUpdatedFiletime(const fs::path& fn, const fs::file_time_type& modify_time);

void set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait = true)
{
    LOG_DEBUG(L"set_last_write_time " + fn.wstring() + L" filetime: " + string_to_wstring(toString(modify_time)));
    std::error_code ec;
    // Bounded retries: transient errors right after a copy settle quickly, but a
    // persistent one (target drive unplugged) must not spin forever - the GUI's
    // stopSync() busy-waits for this thread to finish.
    for (int attempt = 0; attempt < 10; ++attempt) {
        fs::last_write_time(fn, modify_time, ec);
        if (!ec)
            break;
        if (Synchronize::stopRequested())
            return;
        LOG_DEBUG(L"set_last_write_time: " + util::toString(ec.value()))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (ec) {
        LOG_ERROR3(L"Dateizeit konnte nicht gesetzt werden: ", fn.wstring(), string_to_wstring(ec.message()))
        return;
    }
    if (call_wait)
        waitForUpdatedFiletime(fn, modify_time);
}

void waitForUpdatedFiletime(const fs::path& fn, const fs::file_time_type& modify_time)
{
#ifdef _TEST
    ++waitForUpdatedFiletimeCallCount;
#endif
    LOG_DEBUG(L"waitForUpdatedFiletimeL " + fn.wstring() + L" filetime: "+ string_to_wstring(toString(modify_time)));
    std::error_code ec;
    int set_count = 0;
    int end_count = 0;
    while (!util::equal(fs::last_write_time(fn, ec), modify_time)) {
        if (Synchronize::stopRequested())
            return;
        auto actFiletime=fs::last_write_time(fn, ec);
        LOG_DEBUG(L"act write time: " + string_to_wstring(toString(actFiletime)) + L" == " + string_to_wstring(toString(modify_time)))
        set_count++;
        end_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LOG_DEBUG(L"2: " + util::toString(ec.value()) + L"  " + util::toString(set_count)+ L"  " + util::toString(end_count))
        if (set_count > 10) {
            set_last_write_time(fn, modify_time, false);
            LOG_INFO(L"waitForUpdatedFiletime: " + util::toString(end_count))
            set_count=0;
        }
        if (end_count > 100) {
            LOG_DEBUG(L"give up waitForUpdatedFiletime.")
            return;
        }
    }
    LOG_DEBUG(L"endof waitForUpdatedFiletime");
}


CopyResult copyLargeFile(const fs::path& src, const fs::path& dest, uintmax_t fsize)
{
#ifdef _WXWIDGETS
	ProgressDialog* progressDlg = nullptr;
	if (Synchronize::hasInstance()) {
        auto progressDialog = SyncControler::instance().progressDialog();
        if (progressDialog != std::nullopt) {
		    progressDlg = *progressDialog;
            progressDlg->setRange(fsize / TenMB);
            progressDlg->SetTitle(L"Kopiere Datei: " + src.wstring());
        }
	}
#endif
    std::ifstream source(src, std::ios::binary);
    std::ofstream destination(dest, std::ios::binary);
    if (!source.is_open() || !destination.is_open()) {
        return CopyResult::Error;
    }
    std::vector<char> buffer(TenMB);
	uintmax_t copied = 0;
    while (source.read(buffer.data(), buffer.size()) || source.gcount() > 0) {
        if (Synchronize::stopRequested()) {
			LOG_DEBUG(L"Stopped");
			return CopyResult::Stopped;
        }
        destination.write(buffer.data(), source.gcount());
#ifdef _WXWIDGETS
        copied += TenMB;
        if (!Synchronize::stopRequested() && Synchronize::hasInstance() && progressDlg != nullptr) {
			progressDlg->setProgress(copied / TenMB);
        }
#endif
    }
    source.close();
    destination.close();
    return CopyResult::OK;
}

CopyResult copyNormalFile(const wstring& src, const wstring& dest, const SyncItem& si)
{
    if (si.sshSource() && si.sshTarget()) {
        return copySshToSsh(src, dest);
    }
    else if (si.sshSource() && !si.sshTarget()) {
        return copySshToFs(src, dest);
    }
    else if (!si.sshSource() && si.sshTarget()) {
        return copyFsToSsh(src, dest);
    }
    else
        return fs::copy_file(src.c_str(), dest.c_str(), fs::copy_options::update_existing) ? CopyResult::OK : CopyResult::Error;
}

#ifdef WIN32
CopyResult originFilesystemCopyFunction(const wstring& src, const wstring& dest)
{
    if (CopyFile(src.c_str(), dest.c_str(), FALSE)) {
        return CopyResult::OK;
    }
    else {
        DWORD error = GetLastError();
        LOG_ERROR4(L"Fehler beim Kopieren der Datei: ", src, dest, std::to_wstring(error));
        return CopyResult::Error;
    }
}
#else
#endif

#ifdef _WXWIDGETS
CopyResult SyncFile::coreCopyFunction(const wstring& src, const wstring& dest)
{
    // auto filesize = fs::file_size(src);
    // if (filesize > 10U * TenMB)
	//     return copyLargeFile(src, dest, filesize);
	// else
		return copyNormalFile(src, dest, syncItem_);
}
#else
CopyResult SyncFile::coreCopyFunction(const wstring& src, const wstring& dest)
{
	return copyNormalFile(src, dest, syncItem_);
}
#endif

bool SyncFile::saveCopyFile(const wstring& src, const wstring& dest)
{
	LOG_DEBUG(L"saveCopyFile");
	wstring bakFilename = dest + L".bak";
    if (fileExists(dest)) {
        if (fileExists(bakFilename))
            util::remove_file(bakFilename);
        if (!util::rename_file(dest, bakFilename))
            return false;
    }
    CopyResult copyOK = coreCopyFunction(src, dest);
    // SSH skips only the content verification below (TODO); Error/Stopped must
    // still run the cleanup/restore paths, or a failed transfer is reported as
    // success - the caller would then e.g. delete the source (delAftercopy).
    if (ssh_ != SSHPATH::none && copyOK == CopyResult::OK)
        return true;
    if (copyOK == CopyResult::OK && !filesEqual(src, dest))
    {
        LOG_ERROR4(L"Kopiere: ", src, dest, L"Überprüfung der kopierten Datei ergab Abweichungen")
        reportSyncError(L"Kopierte Datei weicht ab: " + dest);
        util::remove_file(dest);
        if (fileExists(bakFilename))
            util::rename_file(bakFilename, dest);
        return false;
    }
    else if (copyOK == CopyResult::Error) {
        LOG_ERROR4(L"Kopiere: ", src, dest, L"Kopieren fehlgeschlagen")
        reportSyncError(L"Kopieren fehlgeschlagen: " + src + L" -> " + dest);
		util::remove_file(dest);
        if (fileExists(bakFilename))
            util::rename_file(bakFilename, dest);
        return false;
	}
	else if (copyOK == CopyResult::Stopped) {
        LOG_INFO2(L"Kopieren abgebrochen: ", src)
			util::remove_file(dest);
		if (fileExists(bakFilename))
			util::rename_file(bakFilename, dest);
		return false;
	}
    return true;
}

void SyncFile::preCopy(const wstring& src, const wstring& dest)
{
    if (ssh_ != SSHPATH::none)
        return; // TODO: Bei SSH erstmal keine Dateiattribute ändern
    if (fileExists(dest)) {
        try {
            fs::permissions(dest, fs::perms::owner_all);
        }
        catch (std::filesystem::filesystem_error const& ex) {
            handleLastError(ex);
        }
        catch (...) {
            LOG_ERROR3(L"Ändere Dateiattribute: ", src, L"Unbekannter Fehler")
        }
        if (syncItem_.fatherSonPrinciple())
            moveFileToBackup(dest);
    }
}


void SyncFile::copySourceToDest(const wstring& source, const wstring& destination, uint64_t size)
{
    LOG_DEBUG(L"copySourceToDest: " + source + L" -> " + destination);
    if (driveNotExist(destination, sshDest()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::setProgressMessage(L"Kopiere \n" + source + L"\nzu\n" + destination + L"...", size);
    #endif
    const wstring src = source;
    const wstring dest = destination;
    try {
        preCopy(src, dest);
        if (!saveCopyFile(src, dest)) {
            return;
        }
        // fs::last_write_time(src) only resolves a local path - an SSH source
        // ("account:/path") made it throw here on every single file, after the
        // copy had already succeeded: the catch below then returned before
        // ++copyCount_, so an SSH-source sync's progress never advanced and
        // reportSyncVolume()/delAftercopy never ran, even though the file had
        // already been copied. See preCopy()'s own TODO for the same gap.
        if (!sshSource()) {
            const auto modify_time = fs::last_write_time(src);
            util::set_last_write_time(dest, modify_time);
        }
        LOG_INFO4(L"Quelle zu Ziel kopiert: ", src, L"nach", dest)
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handleLastError(ex);
        reportSyncError(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        LOG_ERROR4(L"Kopiere: ", src, dest, L"Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler beim Kopieren: " + src + L" -> " + dest);
    }
    ++copyCount_;
    reportSyncVolume(size);
    // An SSH source cannot be deleted through the local filesystem - the call
    // was already a silent no-op for "account:/path" and would now log a
    // spurious recycle bin error instead.
    if (syncItem_.delAftercopy() && !sshSource())
        deleteFile(src);
}

void SyncFile::copyDestToSource(const wstring& destination, const wstring& source, uint64_t size)
{
    LOG_DEBUG(L"copyDestToSource: " + destination + L" -> " + source);
    if (driveNotExist(source, sshSource()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::setProgressMessage(L"Kopiere \n" + destination + L"\nzu\n" + source + L"...", size);
    #endif
    const wstring src = source;
    const wstring dest = destination;
    preCopy(dest, src);
    try {
        // fs::last_write_time(dest) only resolves a local path - see
        // copySourceToDest() for the matching source-side gap. Here it is
        // worse: this call happened before saveCopyFile(), so an SSH target
        // threw before the file was ever copied back, yet still counted as
        // done below.
        fs::file_time_type modify_time{};
        if (!sshDest())
            modify_time = fs::last_write_time(dest);
        if (!saveCopyFile(dest, src))
            return;
        // set_last_write_time() (call_wait defaults to true) already calls
        // waitForUpdatedFiletime() internally; calling it again here doubled
        // the wait - up to ~20s per two-way-sync back-copy instead of ~10s.
        if (!sshDest())
            set_last_write_time(src, modify_time);
        LOG_INFO4(L"Ziel zu Quelle: ", dest, L"nach", src)
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handleLastError(ex);
        reportSyncError(string_to_wstring(ex.what()));
    }
    catch (...) {
        LOG_ERROR4(L"Kopiere: ", dest, src, L"Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler beim Kopieren: " + dest + L" -> " + src);
    }
    ++copyCount_;
    reportSyncVolume(size);
}

using FDResult = FileDifference::Result;

void SyncFile::operator()()
{
    LOG_DEBUG(L"SyncFile::operator()()");
    if (Synchronize::stopRequested())
        return;
    try {
        setSsh();
        wstring source = withoutVolumeSpec(syncItem_.source());
        wstring destination = withoutVolumeSpec(syncItem_.target());
        if (transferFilename_)
            transferFilename(source, destination);
        FileDifference fd(source, destination, syncItem_.compareItems(), ssh_, syncItem_.twoWaySync());
        auto filesize = fd();
        const FDResult fdiff = fd.result();

        const bool deleteTarget = fdiff == FDResult::SourceNotExits && syncItem_.deleteInTarget();
        const bool copyToTarget = fdiff == FDResult::DestNotExists || fdiff == FDResult::SourceModified
            || (fdiff == FDResult::DestModified && syncItem_.copyWhenTargetNewer());
        const bool copyToSource = !copyToTarget && syncItem_.twoWaySync()
            && (fdiff == FDResult::SourceNotExits || fdiff == FDResult::DestModified);

        if (!deleteTarget && !copyToTarget && !copyToSource)
            return;
        // onlyCount_ means "count only" - it must not touch the filesystem, the
        // way SyncBase::syncDirEntry() already handles it for Dir/Tree items.
        if (onlyCount_) {
            ++copyCount_;
            return;
        }
        if (deleteTarget) {
            LOG_INFO4(L"Lösche Zieldatei, da Quelle nicht existiert: ", destination, L"Quelle:", source)
            // util::remove_file() interprets "account:/path" as a bogus local
            // path and silently does nothing for it - deleteInTarget was a
            // complete no-op whenever the target was an SSH path.
            // A remote file cannot go into the local recycle bin without
            // downloading it first, so SSH targets stay a permanent delete.
            bool removed = sshDest() ? sshRemoveFile(destination) : deleteFile(destination);
            if (!removed) {
                LOG_ERROR2(L"Loeschen der Zieldatei fehlgeschlagen: ", destination)
                reportSyncError(L"Loeschen der Zieldatei fehlgeschlagen: " + destination);
            }
        }
        else if (copyToTarget) {
            // Eine leere Quelldatei ist haeufiger ein abgebrochener
            // Schreibvorgang als eine gewollt leere Datei; sie ueber eine
            // vorhandene Zieldatei zu kopieren waere Datenverlust. Wer leere
            // Dateien bewusst mitfuehren will (Platzhalter wie .gitkeep,
            // Sperrdateien), schaltet syncEmptyFiles ein.
            if (fd.sourceSize() > 0 || syncItem_.syncEmptyFiles())
                copySourceToDest(source, destination, filesize);
            else
                LOG_WARNING2(L"Leere Quelldatei wurde nicht kopiert: ", source)
        }
        else if (copyToSource) {
            // The target file is the one being copied here, so its size is the
            // volume moved - filesize is the source's (see FileDifference).
            if (fd.destSize() > 0 || syncItem_.syncEmptyFiles())
                copyDestToSource(destination, source, fd.destSize());
            else
                LOG_WARNING2(L"Leere Zieldatei wurde nicht kopiert: ", destination)
        }
    } catch (std::exception& ex) {
        LOG_ERROR4(L"Kopiere: ", syncItem_.source(), syncItem_.target(), string_to_wstring(ex.what()))
        reportSyncError(string_to_wstring(ex.what()));
    }
    catch (...) {
        LOG_ERROR4(L"Kopiere: ", syncItem_.source(), syncItem_.target(), L" - Unbekannter Fehler")
        reportSyncError(L"Unbekannter Fehler: " + syncItem_.source());
    }
}

} // namespace AFS_Core
