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
#include "Util.h"
#include <FilesystemUtil.h>
#include "FileDifference.h"
#include "Logging.h"
#include "Config.h"
#include "FileDelete.h"
#include "SyncFileSsh.h"
#include "CopyLog.h"

namespace AFS_Core {

namespace fs = std::filesystem;
using std::wstring;
using FDResult = FileDifference::Result;
using util::handle_last_error;
using util::string_to_wstring;
using util::to_string;
using fsu::file_exists;
using util::files_equal;

namespace {
    const size_t TEN_MB{ 10 * 1024 * 1024 };
}

#ifdef _TEST
// Counts calls to the free wait_for_updated_filetime() below, so a regression
// test can catch a caller waiting for the same filetime confirmation twice.
std::atomic<int> wait_for_updated_filetime_call_count{0};
#endif

void move_file_to_backup(const wstring& dest_filename)
{
    log_debug(L"move_file_to_backup: " + dest_filename);
    int i = 1;
    wstring new_dest_filename;
    do {
        std::wstringstream os;
        os << dest_filename << ".bak." << i;
        new_dest_filename = os.str();
        ++i;
    } while (file_exists(new_dest_filename));
    try {
        fs::rename(dest_filename.c_str(), new_dest_filename.c_str());
    }
    catch (fs::filesystem_error& err) {
        std::wstringstream os;
        os << "Fehler: " << err.what() << ", beim Umbenennen von " << dest_filename << " nach " << new_dest_filename;
        throw SyncException(os.str());
    }
}

void transfer_filename(const wstring& source, wstring& dest)
{
    log_debug(L"transfer_filename: " + source + L" -> " + dest);
    dest = util::with_trailing_char(dest, DIR_SEPARATOR);
    fs::path p(source);
    dest += p.filename().wstring();
}

SyncFile::SyncFile(const SyncItem& si, std::size_t& copy_count, bool only_count, bool transfer_filename)
    : SyncBase(si, copy_count, only_count)
    , transfer_filename_(transfer_filename)
{
    set_ssh();
}

void SyncFile::set_ssh()
{
    ssh_ = SSHPATH::none;
    if (sync_item_.ssh_source()) {
        ssh_ = SSHPATH::source;
    } 
    if (sync_item_.ssh_target()) {
        if (ssh_ == SSHPATH::source) {
            ssh_ = SSHPATH::both;
        } else {
            ssh_ = SSHPATH::dest;
        }
    }
}

void wait_for_updated_filetime(const fs::path& fn, const fs::file_time_type& modify_time);

void set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait = true)
{
    log_debug(L"set_last_write_time " + fn.wstring() + L" filetime: " + string_to_wstring(util::to_string(modify_time)));
    std::error_code ec;
    // Bounded retries: transient errors right after a copy settle quickly, but a
    // persistent one (target drive unplugged) must not spin forever - the GUI's
    // stop_sync() busy-waits for this thread to finish.
    for (int attempt = 0; attempt < 10; ++attempt) {
        fs::last_write_time(fn, modify_time, ec);
        if (!ec)
            break;
        if (Synchronize::stopping())
            return;
        log_debug(L"set_last_write_time: " + util::to_string(ec.value()));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (ec) {
        log_error(L"Dateizeit konnte nicht gesetzt werden: ", fn.wstring(), string_to_wstring(ec.message()));
        return;
    }
    if (call_wait)
        wait_for_updated_filetime(fn, modify_time);
}

void wait_for_updated_filetime(const fs::path& fn, const fs::file_time_type& modify_time)
{
#ifdef _TEST
    ++wait_for_updated_filetime_call_count;
#endif
    log_debug(L"waitForUpdatedFiletimeL " + fn.wstring() + L" filetime: "+ string_to_wstring(util::to_string(modify_time)));
    std::error_code ec;
    int set_count = 0;
    int end_count = 0;
    while (!util::equal(fs::last_write_time(fn, ec), modify_time)) {
        if (Synchronize::stopping())
            return;
        auto act_filetime=fs::last_write_time(fn, ec);
        log_debug(L"act write time: " + string_to_wstring(util::to_string(act_filetime)) + L" == " + string_to_wstring(util::to_string(modify_time)));
        set_count++;
        end_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_debug(L"2: " + util::to_string(ec.value()) + L"  " + util::to_string(set_count)+ L"  " + util::to_string(end_count));
        if (set_count > 10) {
            set_last_write_time(fn, modify_time, false);
            log_info(L"wait_for_updated_filetime: " + util::to_string(end_count));
            set_count=0;
        }
        if (end_count > 100) {
            log_debug(L"give up wait_for_updated_filetime.");
            return;
        }
    }
    log_debug(L"endof wait_for_updated_filetime");
}


CopyResult copy_large_file(const fs::path& src, const fs::path& dest, [[maybe_unused]] uintmax_t fsize)
{
#ifdef _WXWIDGETS
	ProgressDialog* progress_dlg = nullptr;
	if (Synchronize::has_instance()) {
        auto progress_dialog = SyncControler::instance().progress_dialog();
        if (progress_dialog != std::nullopt) {
		    progress_dlg = *progress_dialog;
            progress_dlg->setRange(fsize / TEN_MB);
            progress_dlg->SetTitle(L"Kopiere Datei: " + src.wstring());
        }
	}
#endif
    std::ifstream source(src, std::ios::binary);
    std::ofstream destination(dest, std::ios::binary);
    if (!source.is_open() || !destination.is_open()) {
        return CopyResult::Error;
    }
    std::vector<char> buffer(TEN_MB);
    [[maybe_unused]] uintmax_t copied = 0;
    while (source.read(buffer.data(), buffer.size()) || source.gcount() > 0) {
        if (Synchronize::stopping()) {
			log_debug(L"Stopped");
			return CopyResult::Stopped;
        }
        destination.write(buffer.data(), source.gcount());
#ifdef _WXWIDGETS
        copied += TEN_MB;
        if (!Synchronize::stopping() && Synchronize::has_instance() && progress_dlg != nullptr) {
			progress_dlg->setProgress(copied / TEN_MB);
        }
#endif
    }
    source.close();
    destination.close();
    return CopyResult::OK;
}

CopyResult copy_normal_file(const wstring& src, const wstring& dest, const SyncItem& si)
{
    if (si.ssh_source() && si.ssh_target()) {
        return copy_ssh_to_ssh(src, dest);
    }
    else if (si.ssh_source() && !si.ssh_target()) {
        return copy_ssh_to_fs(src, dest);
    }
    else if (!si.ssh_source() && si.ssh_target()) {
        return copy_fs_to_ssh(src, dest);
    }
    else
        return fs::copy_file(src.c_str(), dest.c_str(), fs::copy_options::update_existing) ? CopyResult::OK : CopyResult::Error;
}

#ifdef WIN32
CopyResult origin_filesystem_copy_function(const wstring& src, const wstring& dest)
{
    if (CopyFile(src.c_str(), dest.c_str(), FALSE)) {
        return CopyResult::OK;
    }
    else {
        DWORD error = GetLastError();
        log_error(L"Fehler beim Kopieren der Datei: ", src, dest, std::to_wstring(error));
        return CopyResult::Error;
    }
}
#else
#endif

#ifdef _WXWIDGETS
CopyResult SyncFile::core_copy_function(const wstring& src, const wstring& dest)
{
    // auto filesize = fs::file_size(src);
    // if (filesize > 10U * TEN_MB)
	//     return copy_large_file(src, dest, filesize);
	// else
		return copy_normal_file(src, dest, sync_item_);
}
#else
CopyResult SyncFile::core_copy_function(const wstring& src, const wstring& dest)
{
	return copy_normal_file(src, dest, sync_item_);
}
#endif

bool SyncFile::save_copy_file(const wstring& src, const wstring& dest)
{
	log_debug(L"save_copy_file");
	wstring bak_filename = dest + L".bak";
    if (file_exists(dest)) {
        if (file_exists(bak_filename))
            util::remove_file(bak_filename);
        if (!util::rename_file(dest, bak_filename))
            return false;
    }
    CopyResult copy_ok = core_copy_function(src, dest);
    // SSH skips only the content verification below (TODO); Error/Stopped must
    // still run the cleanup/restore paths, or a failed transfer is reported as
    // success - the caller would then e.g. delete the source (del_aftercopy).
    if (ssh_ != SSHPATH::none && copy_ok == CopyResult::OK)
        return true;
    if (copy_ok == CopyResult::OK && !files_equal(src, dest))
    {
        log_error(L"Kopiere: ", src, dest, L"Überprüfung der kopierten Datei ergab Abweichungen");
        report_sync_error(L"Kopierte Datei weicht ab: " + dest);
        util::remove_file(dest);
        if (file_exists(bak_filename))
            util::rename_file(bak_filename, dest);
        return false;
    }
    else if (copy_ok == CopyResult::Error) {
        log_error(L"Kopiere: ", src, dest, L"Kopieren fehlgeschlagen");
        report_sync_error(L"Kopieren fehlgeschlagen: " + src + L" -> " + dest);
		util::remove_file(dest);
        if (file_exists(bak_filename))
            util::rename_file(bak_filename, dest);
        return false;
	}
	else if (copy_ok == CopyResult::Stopped) {
        log_info(L"Kopieren abgebrochen: ", src);
			util::remove_file(dest);
		if (file_exists(bak_filename))
			util::rename_file(bak_filename, dest);
		return false;
	}
    return true;
}

void SyncFile::pre_copy(const wstring& src, const wstring& dest)
{
    if (ssh_ != SSHPATH::none)
        return; // TODO: file attributes are not adjusted over SSH yet
    if (file_exists(dest)) {
        try {
            fs::permissions(dest, fs::perms::owner_all);
        }
        catch (std::filesystem::filesystem_error const& ex) {
            handle_last_error(ex);
        }
        catch (...) {
            log_error(L"Ändere Dateiattribute: ", src, L"Unbekannter Fehler");
        }
        if (sync_item_.father_son_principle())
            move_file_to_backup(dest);
    }
}


void SyncFile::sync_difference(const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    const FDResult fdiff = fd.result();
    const bool delete_target = fdiff == FDResult::SourceNotExits && sync_item_.delete_in_target();
    const bool copy_to_target = fdiff == FDResult::DestNotExists || fdiff == FDResult::SourceModified
        || (fdiff == FDResult::DestModified && sync_item_.copy_when_target_newer());
    const bool copy_to_source = !copy_to_target && sync_item_.two_way_sync()
        && (fdiff == FDResult::SourceNotExits || fdiff == FDResult::DestModified);
    if (!delete_target && !copy_to_target && !copy_to_source)
        return;
    // only_count_ means "count only" - it must not touch the filesystem, the
    // way SyncBase::sync_dir_entry() already handles it for Dir/Tree items.
    if (only_count_) {
        ++copy_count_;
        return;
    }
    // An empty file is more often an aborted write than an intentionally empty
    // file; copying it over an existing one would lose data. Placeholders like
    // .gitkeep need sync_empty_files.
    if (delete_target)
        delete_target_file(source, destination);
    else if (copy_to_target && (fd.source_size() > 0 || sync_item_.sync_empty_files()))
        copy_source_to_dest(source, destination, filesize);
    else if (copy_to_target)
        log_warning(L"Leere Quelldatei wurde nicht kopiert: ", source);
    // The target file is the one being copied here, so its size is the
    // volume moved - filesize is the source's (see FileDifference).
    else if (fd.dest_size() > 0 || sync_item_.sync_empty_files())
        copy_dest_to_source(destination, source, fd.dest_size());
    else
        log_warning(L"Leere Zieldatei wurde nicht kopiert: ", destination);
}

void SyncFile::delete_target_file(const wstring& source, const wstring& destination)
{
    log_info(L"Lösche Zieldatei, da Quelle nicht existiert: ", destination, L"Quelle:", source);
    // util::remove_file() interprets "account:/path" as a bogus local path and
    // silently does nothing for it. A remote file cannot go into the local
    // recycle bin without downloading it first, so SSH targets stay a permanent delete.
    const bool removed = ssh_dest() ? ssh_remove_file(destination) : delete_file(destination);
    if (!removed) {
        log_error(L"Loeschen der Zieldatei fehlgeschlagen: ", destination);
        report_sync_error(L"Loeschen der Zieldatei fehlgeschlagen: " + destination);
    }
}

void SyncFile::copy_source_to_dest(const wstring& source, const wstring& destination, uint64_t size)
{
    log_debug(L"copy_source_to_dest: " + source + L" -> " + destination);
    if (drive_not_exist(destination, ssh_dest()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(L"Kopiere \n" + source + L"\nzu\n" + destination + L"...", size);
    #endif
    const wstring src = source;
    const wstring dest = destination;
    try {
        pre_copy(src, dest);
        if (!save_copy_file(src, dest)) {
            return;
        }
        // fs::last_write_time(src) only resolves a local path - an SSH source
        // ("account:/path") made it throw here on every single file, after the
        // copy had already succeeded: the catch below then returned before
        // ++copy_count_, so an SSH-source sync's progress never advanced and
        // report_sync_volume()/del_aftercopy never ran, even though the file had
        // already been copied. See pre_copy()'s own TODO for the same gap.
        if (!ssh_source()) {
            const auto modify_time = fs::last_write_time(src);
            util::set_last_write_time(dest, modify_time);
        }
        log_info(L"Quelle zu Ziel kopiert: ", src, L"nach", dest);
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(L"Kopiere: ", src, dest, L"Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler beim Kopieren: " + src + L" -> " + dest);
    }
    ++copy_count_;
    report_sync_volume(size);
    // An SSH source cannot be deleted through the local filesystem - the call
    // was already a silent no-op for "account:/path" and would now log a
    // spurious recycle bin error instead.
    if (sync_item_.del_aftercopy() && !ssh_source())
        delete_file(src);
}

void SyncFile::copy_dest_to_source(const wstring& destination, const wstring& source, uint64_t size)
{
    log_debug(L"copy_dest_to_source: " + destination + L" -> " + source);
    if (drive_not_exist(source, ssh_source()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(L"Kopiere \n" + destination + L"\nzu\n" + source + L"...", size);
    #endif
    const wstring src = source;
    const wstring dest = destination;
    pre_copy(dest, src);
    try {
        // fs::last_write_time(dest) only resolves a local path - see
        // copy_source_to_dest() for the matching source-side gap. Here it is
        // worse: this call happened before save_copy_file(), so an SSH target
        // threw before the file was ever copied back, yet still counted as
        // done below.
        fs::file_time_type modify_time{};
        if (!ssh_dest())
            modify_time = fs::last_write_time(dest);
        if (!save_copy_file(dest, src))
            return;
        // set_last_write_time() (call_wait defaults to true) already calls
        // wait_for_updated_filetime() internally; calling it again here doubled
        // the wait - up to ~20s per two-way-sync back-copy instead of ~10s.
        if (!ssh_dest())
            set_last_write_time(src, modify_time);
        log_info(L"Ziel zu Quelle: ", dest, L"nach", src);
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(L"Kopiere: ", dest, src, L"Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler beim Kopieren: " + dest + L" -> " + src);
    }
    ++copy_count_;
    report_sync_volume(size);
}


void SyncFile::operator()()
{
    log_debug(L"SyncFile::operator()()");
    if (Synchronize::stopping())
        return;
    try {
        set_ssh();
        wstring source = without_volume_spec(sync_item_.source());
        wstring destination = without_volume_spec(sync_item_.target());
        if (transfer_filename_)
            transfer_filename(source, destination);
        FileDifference fd(source, destination, sync_item_.compare_items(), ssh_, sync_item_.two_way_sync());
        const auto filesize = fd();
        sync_difference(fd, source, destination, filesize);
    } catch (std::exception& ex) {
        log_error(L"Kopiere: ", sync_item_.source(), sync_item_.target(), string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(L"Kopiere: ", sync_item_.source(), sync_item_.target(), L" - Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler: " + sync_item_.source());
    }
}

} // namespace AFS_Core
