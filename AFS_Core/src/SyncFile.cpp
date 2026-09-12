#include "pch.h"
#include "SyncFile.h"
#include <filesystem>
#include <sstream>
#include <thread>
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

#ifdef _TEST
std::atomic<int> wait_for_updated_filetime_call_count{0};  // [SyncFile.md#wait_for_updated_filetime_call_count]
#endif

static void move_file_to_backup(const wstring& dest_filename)
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
        fs::rename(dest_filename, new_dest_filename);
    }
    catch (const fs::filesystem_error& err) {
        std::wstringstream os;
        os << "Fehler: " << err.what() << ", beim Umbenennen von " << dest_filename << " nach " << new_dest_filename;
        throw SyncException(os.str());
    }
}

static void transfer_filename(const wstring& source, wstring& dest)
{
    log_debug(L"transfer_filename: " + source + L" -> " + dest);
    dest = util::with_trailing_char(dest, DIR_SEPARATOR) + fs::path(source).filename().wstring();
}

SyncFile::SyncFile(const SyncItem& si, std::size_t& copy_count, bool only_count, bool transfer_filename)
    : SyncBase(si, copy_count, only_count)
    , transfer_filename_(transfer_filename)
{
    set_ssh();
}

void SyncFile::set_ssh()
{
    ssh_ = sync_item_.ssh_path_type();
}

void wait_for_updated_filetime(const fs::path& fn, const fs::file_time_type& modify_time);

void set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait = true)
{
    log_debug(L"set_last_write_time " + fn.wstring() + L" filetime: " + string_to_wstring(util::to_string(modify_time)));
    std::error_code ec;
    for (int attempt = 0; attempt < 10; ++attempt) {  // [SyncFile.md#bounded-retries]
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
    log_debug(L"wait_for_updated_filetime " + fn.wstring() + L" filetime: "+ string_to_wstring(util::to_string(modify_time)));
    std::error_code ec;
    int set_count = 0;
    int end_count = 0;
    while (!util::equal(fs::last_write_time(fn, ec), modify_time)) {
        if (Synchronize::stopping())
            return;
        const auto act_filetime = fs::last_write_time(fn, ec);
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

static CopyResult copy_normal_file(const wstring& src, const wstring& dest, const SyncItem& si)
{
    if (si.ssh_source() && si.ssh_target())
        return copy_ssh_to_ssh(src, dest);
    if (si.ssh_source())
        return copy_ssh_to_fs(src, dest);
    if (si.ssh_target())
        return copy_fs_to_ssh(src, dest);
    return fs::copy_file(src, dest, fs::copy_options::update_existing) ? CopyResult::OK : CopyResult::Error;
}

CopyResult SyncFile::core_copy_function(const wstring& src, const wstring& dest)
{
    return copy_normal_file(src, dest, sync_item_);
}

static void restore_backup(const wstring& dest, const wstring& bak_filename)  // [SyncFile.md#restore_backup]
{
    util::remove_file(dest);
    if (file_exists(bak_filename))
        util::rename_file(bak_filename, dest);
}

bool SyncFile::save_copy_file(const wstring& src, const wstring& dest)
{
    log_debug(L"save_copy_file");
    const wstring bak_filename = dest + L".bak";
    if (file_exists(dest)) {
        if (file_exists(bak_filename))
            util::remove_file(bak_filename);
        if (!util::rename_file(dest, bak_filename))
            return false;
    }
    const CopyResult copy_ok = core_copy_function(src, dest);
    if (ssh_ != SSHPATH::none && copy_ok == CopyResult::OK)  // [SyncFile.md#ssh-skips-verification-only]
        return true;
    if (copy_ok == CopyResult::OK && !files_equal(src, dest)) {
        log_error(L"Kopiere: ", src, dest, L"Überprüfung der kopierten Datei ergab Abweichungen");
        report_sync_error(L"Kopierte Datei weicht ab: " + dest);
        restore_backup(dest, bak_filename);
        return false;
    }
    if (copy_ok == CopyResult::Error) {
        log_error(L"Kopiere: ", src, dest, L"Kopieren fehlgeschlagen");
        report_sync_error(L"Kopieren fehlgeschlagen: " + src + L" -> " + dest);
        restore_backup(dest, bak_filename);
        return false;
    }
    if (copy_ok == CopyResult::Stopped) {
        log_info(L"Kopieren abgebrochen: ", src);
        restore_backup(dest, bak_filename);
        return false;
    }
    return true;
}

void SyncFile::pre_copy(const wstring& src, const wstring& dest)
{
    if (ssh_ != SSHPATH::none)
        return; // TODO: file attributes are not adjusted over SSH yet
    if (!file_exists(dest))
        return;
    std::error_code ec;
    fs::permissions(dest, fs::perms::owner_all, ec);
    if (ec)
        log_error(L"Ändere Dateiattribute: ", src, string_to_wstring(ec.message()));
    if (sync_item_.father_son_principle())
        move_file_to_backup(dest);
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
    if (only_count_) {  // [SyncFile.md#only-count-touches-nothing]
        ++copy_count_;
        return;
    }
    if (delete_target)  // [SyncFile.md#empty-source-file]
        delete_target_file(source, destination);
    else if (copy_to_target && (fd.source_size() > 0 || sync_item_.sync_empty_files()))
        copy_source_to_dest(source, destination, filesize);
    else if (copy_to_target)
        log_warning(L"Leere Quelldatei wurde nicht kopiert: ", source);
    else if (fd.dest_size() > 0 || sync_item_.sync_empty_files())  // [SyncFile.md#volume-is-target-size]
        copy_dest_to_source(destination, source, fd.dest_size());
    else
        log_warning(L"Leere Zieldatei wurde nicht kopiert: ", destination);
}

void SyncFile::delete_target_file(const wstring& source, const wstring& destination)
{
    log_info(L"Lösche Zieldatei, da Quelle nicht existiert: ", destination, L"Quelle:", source);
    const bool removed = ssh_dest() ? ssh_remove_file(destination) : delete_file(destination);  // [SyncFile.md#ssh-target-delete]
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
    try {  // [SyncFile.md#failures-return-early]
        pre_copy(source, destination);
        if (!save_copy_file(source, destination))
            return;
        if (!ssh_source())  // [SyncFile.md#local-only-filetime]
            util::set_last_write_time(destination, fs::last_write_time(source));
        log_info(L"Quelle zu Ziel kopiert: ", source, L"nach", destination);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (const std::exception& ex) {
        log_error(L"Kopiere: ", source, destination, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(L"Kopiere: ", source, destination, L"Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler beim Kopieren: " + source + L" -> " + destination);
        return;
    }
    ++copy_count_;
    report_sync_volume(size);
    if (sync_item_.del_aftercopy() && !ssh_source())  // an SSH source cannot be deleted through the local filesystem
        delete_file(source);
}

void SyncFile::copy_dest_to_source(const wstring& destination, const wstring& source, uint64_t size)
{
    log_debug(L"copy_dest_to_source: " + destination + L" -> " + source);
    if (drive_not_exist(source, ssh_source()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(L"Kopiere \n" + destination + L"\nzu\n" + source + L"...", size);
    #endif
    try {
        pre_copy(destination, source);
        fs::file_time_type modify_time{};  // fs::last_write_time() only resolves a local path - see copy_source_to_dest()
        if (!ssh_dest())
            modify_time = fs::last_write_time(destination);
        if (!save_copy_file(destination, source))
            return;
        if (!ssh_dest())
            set_last_write_time(source, modify_time);
        log_info(L"Ziel zu Quelle: ", destination, L"nach", source);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (const std::exception& ex) {
        log_error(L"Kopiere: ", destination, source, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(L"Kopiere: ", destination, source, L"Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler beim Kopieren: " + destination + L" -> " + source);
        return;
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
        wstring source = without_volume_spec(sync_item_.source());
        wstring destination = without_volume_spec(sync_item_.target());
        if (transfer_filename_)
            transfer_filename(source, destination);
        FileDifference fd(source, destination, sync_item_.compare_items(), ssh_, sync_item_.two_way_sync());
        const auto filesize = fd();
        sync_difference(fd, source, destination, filesize);
    } catch (const std::exception& ex) {
        log_error(L"Kopiere: ", sync_item_.source(), sync_item_.target(), string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(L"Kopiere: ", sync_item_.source(), sync_item_.target(), L" - Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler: " + sync_item_.source());
    }
}

} // namespace AFS_Core
