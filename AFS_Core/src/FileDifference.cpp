#include "pch.h"
#include "FileDifference.h"
#include "Synchronize.h"
#include "Util.h"
#include <filesystem>
#include <FilesystemUtil.h>
#include "Logging.h"
#include "SshFileInspector.h"
#include "Translation.h"
#include "Config.h"

namespace AFS_Core {

namespace fs=std::filesystem;

using std::wstring;
using namespace util;

static wstring to_wstring(FileDifference::Result r)
{
    using Result = FileDifference::Result;
    switch (r) {
    case Result::Equal: return L"Equal";
    case Result::SourceModified: return L"SourceModified";
    case Result::DestModified: return L"DestModified";
    case Result::DestNotExists: return L"DestNotExists";
    case Result::SourceNotExits: return L"SourceNotExits";
    case Result::SourceDriveNotMounted: return L"SourceDriveNotMounted";
    case Result::DestDriveNotMounted: return L"DestDriveNotMounted";
    case Result::Undefined: break;
    }
    return L"Undefined";
}

FileDifference::FileDifference(const wstring& source, const wstring& dest, const CompareItems& ci, SSHPATH ssh, bool two_way_sync)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
    , ssh_(ssh)
    , two_way_sync_(two_way_sync)
{}


static bool read_ssh_file_info(const std::wstring& fname, bool& drive_mounted, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    const auto colon_pos = fname.find(L':');
    if (colon_pos == std::wstring::npos || colon_pos == 0) {
        log_error(translation::translate("ssh_path_invalid") + fname);
        drive_mounted = false; // malformed, not "confirmed absent" - must not look like a deletion-safe SourceNotExits
        return false;
    }
    const std::wstring ssh_name = fname.substr(0, colon_pos);
    const std::wstring remote_path = fname.substr(colon_pos + 1);
    const std::optional<SshAccount> account = Config::instance().find_ssh_account_by_name(util::wstring_to_string(ssh_name));
    if (!account.has_value()) {
        log_error(translation::translate("ssh_account_not_found") + ssh_name);
        drive_mounted = false;
        return false;
    }
    try {
        SshFileInspector inspector(account.value());
        const RemoteFileInfo info = inspector.inspect_file(wstring_to_string(remote_path));
        if (!info.exists)
            return false; // successfully connected and confirmed absent - safe to treat as SourceNotExits
        filesize = info.size_bytes;
        const auto since_epoch = std::chrono::system_clock::from_time_t(info.raw_mtime).time_since_epoch();
        last_write_time = fs::file_time_type(std::chrono::duration_cast<fs::file_time_type::duration>(since_epoch));
        drive_mounted = true;
        return true;
    }
    catch (const std::exception& e) {
        log_error(translation::translate("ssh_read_error_for") + fname + L": " + string_to_wstring(e.what()));  // [FileDifference.md#ssh-error-is-not-absence]
        drive_mounted = false;
        return false;
    }
}

static bool read_file_info(const std::wstring& fname, bool& drive_mounted, uint64_t& filesize, fs::file_time_type& last_write_time, bool ssh)
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
        os << translation::translate("comparing") << L'\n' << source_ << L'\n' << translation::translate("comparing_with") << L'\n' << dest_ << L"...";
        Synchronize::set_progress_message(os.str(), sourcesize_);
    }
    if (util::files_equal(source_, dest_))
        return;
    result_ = two_way_sync_ && util::greater(desttime, sourcetime) ? Result::DestModified : Result::SourceModified;
}

} // namespace AFS_Core
