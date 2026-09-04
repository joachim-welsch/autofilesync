#include "pch.h"
#include "FileDifference.h"
#include "Synchronize.h"
#include "util.h"
#include <filesystem>
#include <FilesystemUtil.h>
#include "Logging.h"
#include "SshFileInspector.h"
#include "Config.h"

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

FileDifference::FileDifference(const wstring& source, const wstring& dest, const CompareItems& ci, SSHPATH ssh, bool twoWaySync)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
    , ssh_(ssh)
    , twoWaySync_(twoWaySync)
{}


bool readSshFileInfo(const std::wstring& fname, bool& driveMounted, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    std::wstring wfname = fname;
    auto colonPos = wfname.find(L':');
    if (colonPos == std::wstring::npos || colonPos == 0) {
        LOG_ERROR(L"Ungueltiger SSH-Pfad: " + fname);
        driveMounted = false; // malformed, not "confirmed absent" - must not look like a deletion-safe SourceNotExits
        return false;
    }

    std::wstring sshName = wfname.substr(0, colonPos);
    std::wstring remotePath = wfname.substr(colonPos + 1);
    std::optional<SshAccount> account = Config::instance().findSshAccountByName(util::wstring_to_string(sshName));
    if (!account.has_value()) {
        LOG_ERROR(L"SSH-Account nicht gefunden: " + sshName);
        driveMounted = false;
        return false;
    }
    try {
        SshFileInspector inspector(account.value());
        std::string remotePathStr = wstring_to_string(remotePath);
        RemoteFileInfo info = inspector.inspectFile(remotePathStr);
        if (!info.exists) {
            return false; // successfully connected and confirmed absent - safe to treat as SourceNotExits
        }
        filesize = info.size_bytes;
        auto sys_time = std::chrono::system_clock::from_time_t(info.raw_mtime);
        auto duration = sys_time.time_since_epoch();
        last_write_time = std::filesystem::file_time_type(std::chrono::duration_cast<std::filesystem::file_time_type::duration>(duration));
        driveMounted = true;
        return true;
    }
    catch (const std::exception& e) {
        // Connection/auth failure, not a confirmed-absent file: must not be
        // mistaken for "source deleted", or SyncFile deletes the destination
        // (Fehler 2) the moment deleteInTarget() is enabled.
        LOG_ERROR(L"SSH-Fehler beim Lesen von " + fname + L": " + string_to_wstring(e.what()));
        driveMounted = false;
        return false;
    }
}

bool readFileInfo(const std::wstring& fname, bool& driveMounted, uint64_t& filesize, fs::file_time_type& last_write_time, bool ssh)
{
    if (ssh)
        return readSshFileInfo(fname, driveMounted, filesize, last_write_time);
    fsu::FileInfoResult result = fsu::fileinfo(fname, filesize, last_write_time);
    if (result != fsu::FileInfoResult::OK) {
        if (result == fsu::FileInfoResult::DriveNotMounted)
            driveMounted = false;
        const wstring errorMessage = fsu::makeErrorMessage(result) + L": ";
        LOG_INFO2(errorMessage, fname)
        return false;
    }
    return true;
}

uint64_t FileDifference::operator()()
{
    fs::file_time_type sourcetime, desttime;
    bool sourceDriveMounted = true, destDriveMounted = true;
    // Both sides are always read, even if one is already known missing: the
    // old "else if" short-circuited the dest read entirely whenever the
    // source was missing, so destSize()/destination mtime silently stayed at
    // 0/default for a SourceNotExits result - exactly the case
    // copyDestToSource() (twoWaySync picking up a target-only file) needs a
    // real size for. It skipped the "Leere Zieldatei wurde nicht kopiert"
    // guard in SyncFile::operator()(), silently never copying anything back.
    const bool sourceOk = readFileInfo(source_, sourceDriveMounted, sourcesize_, sourcetime, ssh_ == SSHPATH::source || ssh_ == SSHPATH::both);
    const bool destOk = readFileInfo(dest_, destDriveMounted, destsize_, desttime, ssh_ == SSHPATH::dest || ssh_ == SSHPATH::both);

    if (!sourceOk)
        result_ = sourceDriveMounted ? Result::SourceNotExits : Result::SourceDriveNotMounted;
    else if (!destOk)
        result_ = destDriveMounted ? Result::DestNotExists : Result::DestDriveNotMounted;
    else if (ci_.time() && util::greater(desttime, sourcetime))
        result_ = Result::DestModified;
    else if (ci_.time() && util::greater(sourcetime, desttime))
        result_ = Result::SourceModified;
    else if (ci_.size() && sourcesize_ > destsize_)
        result_ = Result::SourceModified;
    else if (ci_.size() && destsize_ > sourcesize_)
        result_ = Result::DestModified;
    if (result_ == Result::Equal && ci_.content()) {
        if (ssh_ != SSHPATH::none) {
            LOG_DEBUG(L"Content-Vergleich fuer SSH-Pfade uebersprungen");
        } else {
            if (sourcesize_ > 1000000) {
                std::wostringstream os;
                os << "Vergleiche\n" << source_ << "\nmit\n" << dest_ << "...";
                Synchronize::setProgressMessage(os.str(), sourcesize_);
            }
            if (!util::filesEqual(source_, dest_)) {
                // Content alone can't say *which* side changed. For one-way
                // sync that's fine - SourceModified always drives the copy
                // toward the target, which is exactly the intended direction.
                // For two-way sync, blindly picking SourceModified silently
                // discards a destination-only edit whenever only content
                // comparison is configured, so fall back to mtime to resolve
                // direction: the two readFileInfo() calls above already
                // fetched sourcetime/desttime regardless of ci_.time().
                if (twoWaySync_ && util::greater(desttime, sourcetime))
                    result_ = Result::DestModified;
                else
                    result_ = Result::SourceModified;
            }
        }
    }
    LOG_DEBUG(wstring(L"FileDifference result: ") + to_wstring(result_))
    return sourcesize_;
}
