/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
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
    case Result::ContentConflict: return L"ContentConflict";
    case Result::SourceDeleted: return L"SourceDeleted";
    case Result::DestDeleted: return L"DestDeleted";
    case Result::BothModified: return L"BothModified";
    case Result::NeitherExists: return L"NeitherExists";
    case Result::Undefined: break;
    }
    return L"Undefined";
}

FileDifference::FileDifference(const wstring& source, const wstring& dest, const CompareItems& ci, SSHPATH ssh, bool two_way_sync, const FileState* base)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
    , ssh_(ssh)
    , two_way_sync_(two_way_sync)
    , base_(two_way_sync ? base : nullptr)  // [FileDifference.md#base-only-two-way]
{
    if (ssh_ != SSHPATH::none && ci_.content()) {  // [FileDifference.md#ssh-content-fallback]
        ci_.content(false);
        ci_.size(true);
        ci_.time(true);
        log_debug(L"content comparison is not available for SSH paths - comparing size and time instead");
    }
}

static bool read_ssh_file_info(const std::wstring& fname, bool& drive_mounted, bool& absent, uint64_t& filesize, fs::file_time_type& last_write_time)
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
        if (!info.exists) {
            absent = true; // successfully connected and confirmed absent - safe to treat as SourceNotExits
            return false;
        }
        filesize = info.size_bytes;
        last_write_time = util::to_file_time(info.raw_mtime);  // [FileDifference.md#remote-time-conversion]
        drive_mounted = true;
        return true;
    }
    catch (const SshUnreachable& e) {
        log_warning(translation::translate("ssh_not_reachable_for") + fname + L": " + string_to_wstring(e.what()));  // [FileDifference.md#unreachable-is-a-warning]
        drive_mounted = false;
        return false;
    }
    catch (const std::exception& e) {
        log_error(translation::translate("ssh_read_error_for") + fname + L": " + string_to_wstring(e.what()));  // [FileDifference.md#ssh-error-is-not-absence]
        drive_mounted = false;
        return false;
    }
}

static bool read_file_info(const std::wstring& fname, bool& drive_mounted, bool& absent, uint64_t& filesize, fs::file_time_type& last_write_time, bool ssh)
{
    if (ssh)
        return read_ssh_file_info(fname, drive_mounted, absent, filesize, last_write_time);
    fsu::FileInfoResult result = fsu::fileinfo(fname, filesize, last_write_time);
    if (result != fsu::FileInfoResult::OK) {
        if (result == fsu::FileInfoResult::DriveNotMounted)
            drive_mounted = false;
        absent = result == fsu::FileInfoResult::FileNotExist;  // [FileDifference.md#absent]
        const wstring error_message = fsu::make_error_message(result) + L": ";
        log_info(error_message, fname);
        return false;
    }
    return true;
}

std::optional<ObservedFile> observe_file(const wstring& path, bool ssh)  // [FileDifference.md#observe_file]
{
    bool drive_mounted = true, absent = false;
    ObservedFile observed;
    if (!read_file_info(path, drive_mounted, absent, observed.size, observed.time, ssh))
        return std::nullopt;
    return observed;
}

uint64_t FileDifference::operator()()
{
    bool source_drive_mounted = true, dest_drive_mounted = true, source_absent = false, dest_absent = false;
    const bool source_ok = read_file_info(source_, source_drive_mounted, source_absent, sourcesize_, sourcetime_, ssh_ == SSHPATH::source || ssh_ == SSHPATH::both);
    const bool dest_ok = read_file_info(dest_, dest_drive_mounted, dest_absent, destsize_, desttime_, ssh_ == SSHPATH::dest || ssh_ == SSHPATH::both);
    if (!source_ok)
        result_ = !source_drive_mounted ? Result::SourceDriveNotMounted : source_absent ? source_missing(dest_absent) : Result::SourceNotExits;
    else if (!dest_ok)
        result_ = !dest_drive_mounted ? Result::DestDriveNotMounted : dest_absent ? dest_missing() : Result::DestNotExists;
    else
        result_ = base_ ? compare_with_base() : compare_metadata(sourcetime_, desttime_);
    if (result_ == Result::Equal && ci_.content())
        compare_content(sourcetime_, desttime_);
    log_debug(wstring(L"FileDifference result: ") + to_wstring(result_));
    return sourcesize_;
}

FileDifference::Result FileDifference::source_missing(bool dest_absent) const  // [FileDifference.md#three-way]
{
    if (dest_absent)
        return Result::NeitherExists;
    return base_ && unchanged(destsize_, desttime_, base_->target_time) ? Result::SourceDeleted : Result::SourceNotExits;
}

FileDifference::Result FileDifference::dest_missing() const
{
    return base_ && unchanged(sourcesize_, sourcetime_, base_->source_time) ? Result::DestDeleted : Result::DestNotExists;
}

bool FileDifference::unchanged(uint64_t size, fs::file_time_type time, fs::file_time_type base_time) const  // [FileDifference.md#unchanged]
{
    return size == base_->size && !newer(time, base_time) && !newer(base_time, time);
}

bool FileDifference::alike() const  // [FileDifference.md#both-changed-alike]
{
    return sourcesize_ == destsize_ && !newer(sourcetime_, desttime_) && !newer(desttime_, sourcetime_);
}

FileDifference::Result FileDifference::compare_with_base() const  // [FileDifference.md#three-way]
{
    const bool source_changed = !unchanged(sourcesize_, sourcetime_, base_->source_time);
    const bool dest_changed = !unchanged(destsize_, desttime_, base_->target_time);
    if (source_changed && dest_changed)  // [FileDifference.md#both-changed-alike]
        return alike() ? Result::Equal : Result::BothModified;
    if (source_changed)
        return Result::SourceModified;
    return dest_changed ? Result::DestModified : Result::Equal;
}

bool FileDifference::newer(fs::file_time_type a, fs::file_time_type b) const  // [FileDifference.md#time-tolerance]
{
    const std::chrono::seconds tolerance(ci_.time_tolerance_seconds());
    if (ssh_ == SSHPATH::none)
        return util::greater(a, b + std::chrono::duration_cast<fs::file_time_type::duration>(tolerance));
    return util::to_time_t(a) > util::to_time_t(b) + tolerance.count();  // [FileDifference.md#second-resolution]
}

FileDifference::Result FileDifference::compare_metadata(fs::file_time_type sourcetime, fs::file_time_type desttime) const
{
    if (ci_.time() && newer(desttime, sourcetime))
        return Result::DestModified;
    if (ci_.time() && newer(sourcetime, desttime))
        return Result::SourceModified;
    if (ci_.size() && sourcesize_ > destsize_)
        return Result::SourceModified;
    if (ci_.size() && destsize_ > sourcesize_)
        return Result::DestModified;
    return Result::Equal;
}

void FileDifference::compare_content(fs::file_time_type sourcetime, fs::file_time_type desttime)  // [FileDifference.md#ssh-content-fallback]
{
    if (sourcesize_ > 1000000) {
        std::wostringstream os;
        os << translation::translate("comparing") << L'\n' << source_ << L'\n' << translation::translate("comparing_with") << L'\n' << dest_ << L"...";
        Synchronize::set_progress_message(os.str(), L"", sourcesize_);
    }
    if (util::files_equal(source_, dest_))
        return;
    if (two_way_sync_ && sourcesize_ == destsize_ && !newer(sourcetime, desttime) && !newer(desttime, sourcetime))
        result_ = Result::ContentConflict;  // [FileDifference.md#content-conflict]
    else
        result_ = two_way_sync_ && newer(desttime, sourcetime) ? Result::DestModified : Result::SourceModified;
}

} // namespace AFS_Core
