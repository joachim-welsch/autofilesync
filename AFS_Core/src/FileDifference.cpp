#include "pch.h"
#include "FileDifference.h"
#include "Synchronize.h"
#include "Util.h"
#include <filesystem>
#include <FilesystemUtil.h>
#include "Logging.h"
#include "SshFileInspector.h"
#include "Config.h"

namespace AFS_Core {

namespace fs=std::filesystem;

using std::wstring;
using namespace util;

wstring to_wstring(FileDifference::Result r)
{
    if (r==FileDifference::Result::Equal)
        return L"Equal";
    else if (r==FileDifference::Result::SourceModified)
        return L"SourceModified";
    else if (r==FileDifference::Result::DestModified)
        return L"DestModified";
    else if (r==FileDifference::Result::DestNotExists)
        return L"DestNotExists";
    else if (r==FileDifference::Result::SourceNotExits)
        return L"SourceNotExits";
    else
        return L"Undefined";
}

FileDifference::FileDifference(const wstring& source, const wstring& dest, const CompareItems& ci, SSHPATH ssh, bool two_way_sync)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
    , ssh_(ssh)
    , two_way_sync_(two_way_sync)
{}


bool read_ssh_file_info(const std::wstring& fname, bool& drive_mounted, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    std::wstring wfname = fname;
    auto colon_pos = wfname.find(L':');
    if (colon_pos == std::wstring::npos || colon_pos == 0) {
        log_error(L"Ungueltiger SSH-Pfad: " + fname);
        drive_mounted = false; // malformed, not "confirmed absent" - must not look like a deletion-safe SourceNotExits
        return false;
    }

    std::wstring ssh_name = wfname.substr(0, colon_pos);
    std::wstring remote_path = wfname.substr(colon_pos + 1);
    std::optional<SshAccount> account = Config::instance().find_ssh_account_by_name(util::wstring_to_string(ssh_name));
    if (!account.has_value()) {
        log_error(L"SSH-Account nicht gefunden: " + ssh_name);
        drive_mounted = false;
        return false;
    }
    try {
        SshFileInspector inspector(account.value());
        std::string remote_path_str = wstring_to_string(remote_path);
        RemoteFileInfo info = inspector.inspect_file(remote_path_str);
        if (!info.exists) {
            return false; // successfully connected and confirmed absent - safe to treat as SourceNotExits
        }
        filesize = info.size_bytes;
        auto sys_time = std::chrono::system_clock::from_time_t(info.raw_mtime);
        auto duration = sys_time.time_since_epoch();
        last_write_time = std::filesystem::file_time_type(std::chrono::duration_cast<std::filesystem::file_time_type::duration>(duration));
        drive_mounted = true;
        return true;
    }
    catch (const std::exception& e) {
        // Connection/auth failure, not a confirmed-absent file: must not be
        // mistaken for "source deleted", or SyncFile deletes the destination
        // (Fehler 2) the moment delete_in_target() is enabled.
        log_error(L"SSH-Fehler beim Lesen von " + fname + L": " + string_to_wstring(e.what()));
        drive_mounted = false;
        return false;
    }
}

bool read_file_info(const std::wstring& fname, bool& drive_mounted, uint64_t& filesize, fs::file_time_type& last_write_time, bool ssh)
{
    if (ssh)
        return read_ssh_file_info(fname, drive_mounted, filesize, last_write_time);
    fsu::FileInfoResult result = fsu::fileinfo(fname, filesize, last_write_time);
    if (result != fsu::FileInfoResult::OK) {
        if (result == fsu::FileInfoResult::DriveNotMounted)
            drive_mounted = false;
        const wstring error_message = fsu::make_error_message(result) + L": ";
        log_info(error_message, fname);
        return false;
    }
    return true;
}

uint64_t FileDifference::operator()()
{
    fs::file_time_type sourcetime, desttime;
    bool source_drive_mounted = true, dest_drive_mounted = true;
    // Both sides are always read, even if one is already known missing: the
    // old "else if" short-circuited the dest read entirely whenever the
    // source was missing, so dest_size()/destination mtime silently stayed at
    // 0/default for a SourceNotExits result - exactly the case
    // copy_dest_to_source() (two_way_sync picking up a target-only file) needs a
    // real size for. It skipped the "Leere Zieldatei wurde nicht kopiert"
    // guard in SyncFile::operator()(), silently never copying anything back.
    const bool source_ok = read_file_info(source_, source_drive_mounted, sourcesize_, sourcetime, ssh_ == SSHPATH::source || ssh_ == SSHPATH::both);
    const bool dest_ok = read_file_info(dest_, dest_drive_mounted, destsize_, desttime, ssh_ == SSHPATH::dest || ssh_ == SSHPATH::both);

    if (!source_ok)
        result_ = source_drive_mounted ? Result::SourceNotExits : Result::SourceDriveNotMounted;
    else if (!dest_ok)
        result_ = dest_drive_mounted ? Result::DestNotExists : Result::DestDriveNotMounted;
    else
        result_ = compare_metadata(sourcetime, desttime);
    if (result_ == Result::Equal && ci_.content())
        compare_content(sourcetime, desttime);
    log_debug(wstring(L"FileDifference result: ") + to_wstring(result_));
    return sourcesize_;
}

FileDifference::Result FileDifference::compare_metadata(fs::file_time_type sourcetime, fs::file_time_type desttime) const
{
    if (ci_.time() && util::greater(desttime, sourcetime))
        return Result::DestModified;
    if (ci_.time() && util::greater(sourcetime, desttime))
        return Result::SourceModified;
    if (ci_.size() && sourcesize_ > destsize_)
        return Result::SourceModified;
    if (ci_.size() && destsize_ > sourcesize_)
        return Result::DestModified;
    return Result::Equal;
}

void FileDifference::compare_content(fs::file_time_type sourcetime, fs::file_time_type desttime)
{
    if (ssh_ != SSHPATH::none) {
        log_debug(L"Content-Vergleich fuer SSH-Pfade uebersprungen");
        return;
    }
    if (sourcesize_ > 1000000) {
        std::wostringstream os;
        os << "Vergleiche\n" << source_ << "\nmit\n" << dest_ << "...";
        Synchronize::set_progress_message(os.str(), sourcesize_);
    }
    if (util::files_equal(source_, dest_))
        return;
    // Content alone can't say *which* side changed. For one-way sync that's
    // fine - SourceModified always drives the copy toward the target. For
    // two-way sync, blindly picking SourceModified would silently discard a
    // destination-only edit whenever only content comparison is configured,
    // so mtime resolves the direction; read_file_info() fetched the times
    // regardless of ci_.time().
    result_ = two_way_sync_ && util::greater(desttime, sourcetime) ? Result::DestModified : Result::SourceModified;
}

} // namespace AFS_Core
