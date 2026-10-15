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
#include "SyncFile.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>
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
#include "Translation.h"

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
class ReportedOnce {  // [SyncFile.md#reported-once]
    std::mutex mutex_;
    std::set<wstring> reported_;
public:
    bool first_report(const wstring& destination, bool applies)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (applies)
            return reported_.insert(destination).second;
        reported_.erase(destination);
        return false;
    }
};

ReportedOnce newer_targets;
ReportedOnce content_conflicts;

void report_newer_target(const wstring& source, const wstring& destination, bool target_newer)
{
    if (newer_targets.first_report(destination, target_newer))
        log_info(translation::translate("target_newer_not_copied"), source, L"->", destination);
}

void report_content_conflict(const wstring& source, const wstring& destination, bool conflict)  // [SyncFile.md#content-conflict]
{
    if (!content_conflicts.first_report(destination, conflict))
        return;
    log_error(translation::translate("content_conflict_not_copied"), source, L"<->", destination);
    report_sync_error(translation::translate("content_conflict_not_copied") + L" " + source + L" <-> " + destination);
}
}

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
        throw SyncException(translation::translate("rename_error",
            {string_to_wstring(err.what()), dest_filename, new_dest_filename}));
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
        log_error(translation::translate("file_time_not_settable"), fn.wstring(), string_to_wstring(ec.message()));
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

constexpr std::size_t COPY_BLOCK_BYTES = 1 << 20;

static CopyResult copy_local_file(const wstring& src, const wstring& dest)  // [SyncFile.md#copy_local_file]
{
    std::ifstream in(fs::path(src), std::ios::binary);
    std::ofstream out(fs::path(dest), std::ios::binary | std::ios::trunc);
    if (!in || !out)
        return CopyResult::Error;
    std::vector<char> block(COPY_BLOCK_BYTES);
    uint64_t copied = 0;
    while (in.read(block.data(), static_cast<std::streamsize>(block.size())) || in.gcount() > 0) {
        if (Synchronize::stopping())
            return CopyResult::Stopped;
        if (!out.write(block.data(), in.gcount()))
            return CopyResult::Error;
        Synchronize::report_copied_bytes(copied += static_cast<uint64_t>(in.gcount()));
    }
    if (in.bad())
        return CopyResult::Error;
    out.close();
    if (!out)
        return CopyResult::Error;
    std::error_code ec;
    fs::permissions(dest, fs::status(src, ec).permissions(), ec);  // [SyncFile.md#copy_local_file]
    return CopyResult::OK;
}

static CopyResult copy_normal_file(const wstring& src, const wstring& dest)  // [SyncFile.md#copy-direction-follows-the-paths]
{
    const bool ssh_src = get_account_from_path(src).has_value();
    const bool ssh_dest = get_account_from_path(dest).has_value();
    if (ssh_src && ssh_dest)
        return copy_ssh_to_ssh(src, dest);
    if (ssh_src)
        return copy_ssh_to_fs(src, dest);
    if (ssh_dest)
        return copy_fs_to_ssh(src, dest);
    return copy_local_file(src, dest);
}

CopyResult SyncFile::core_copy_function(const wstring& src, const wstring& dest)
{
    return copy_normal_file(src, dest);
}

static void restore_backup(const wstring& dest, const wstring& bak_filename)  // [SyncFile.md#restore_backup]
{
    util::remove_file(dest);
    if (file_exists(bak_filename))
        util::rename_file(bak_filename, dest);
}

static void discard_backup(const wstring& bak_filename)  // [SyncFile.md#discard_backup]
{
    if (file_exists(bak_filename))
        util::remove_file(bak_filename);
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
    if (ssh_ != SSHPATH::none && copy_ok == CopyResult::OK) {  // [SyncFile.md#ssh-skips-verification-only]
        discard_backup(bak_filename);
        return true;
    }
    if (copy_ok == CopyResult::OK && !files_equal(src, dest)) {
        log_error(translation::translate("copying_label"), src, dest, translation::translate("copy_verification_differs"));
        report_sync_error(translation::translate("copied_file_differs") + dest);
        restore_backup(dest, bak_filename);
        return false;
    }
    if (copy_ok == CopyResult::Error) {
        log_error(translation::translate("copying_label"), src, dest, translation::translate("copy_failed"));
        report_sync_error(translation::translate("copy_failed_for") + src + L" -> " + dest);
        restore_backup(dest, bak_filename);
        return false;
    }
    if (copy_ok == CopyResult::Stopped) {
        log_info(translation::translate("copy_cancelled"), src);
        restore_backup(dest, bak_filename);
        return false;
    }
    discard_backup(bak_filename);
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
        log_error(translation::translate("file_attributes_change"), src, string_to_wstring(ec.message()));
    if (sync_item_.father_son_principle())
        move_file_to_backup(dest);
}

std::optional<PlannedAction> SyncFile::decide(const FileDifference& fd) const  // [SyncFile.md#decide]
{
    const FDResult fdiff = fd.result();
    if (fdiff == FDResult::SourceNotExits && sync_item_.delete_in_target())
        return PlannedAction::DeleteInTarget;
    const bool copy_to_target = fdiff == FDResult::DestNotExists || fdiff == FDResult::SourceModified
        || (fdiff == FDResult::DestModified && sync_item_.copy_when_target_newer());
    if (copy_to_target)
        return sync_item_.del_aftercopy() && !ssh_source() ? PlannedAction::MoveToTarget : PlannedAction::CopyToTarget;
    if (sync_item_.two_way_sync() && (fdiff == FDResult::SourceNotExits || fdiff == FDResult::DestModified))
        return PlannedAction::CopyToSource;
    return std::nullopt;
}

bool SyncFile::empty_file_skipped(const FileDifference& fd, PlannedAction action) const  // [SyncFile.md#empty-source-file]
{
    if (sync_item_.sync_empty_files() || action == PlannedAction::DeleteInTarget)
        return false;
    return (action == PlannedAction::CopyToSource ? fd.dest_size() : fd.source_size()) == 0;
}

void SyncFile::sync_difference(const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    const std::optional<PlannedAction> action = decide(fd);
    if (!action) {
        report_newer_target(source, destination, fd.result() == FDResult::DestModified);
        report_content_conflict(source, destination, fd.result() == FDResult::ContentConflict);
        return;
    }
    if (only_count_) {  // [SyncFile.md#only-count-touches-nothing]
        if (empty_file_skipped(fd, *action))
            return;
        ++copy_count_;
        plan_step({sync_item_.name(), *action, source, destination, file_in_target(*action) ? fd.dest_size() : fd.source_size()});
        return;
    }
    carry_out(*action, fd, source, destination, filesize);
}

void SyncFile::carry_out(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    if (action == PlannedAction::DeleteInTarget)
        delete_target_file(source, destination);
    else if (empty_file_skipped(fd, action))
        log_warning(translation::translate(action == PlannedAction::CopyToSource ? "empty_target_not_copied" : "empty_source_not_copied"),
            action == PlannedAction::CopyToSource ? destination : source);
    else if (action == PlannedAction::CopyToSource)  // [SyncFile.md#volume-is-target-size]
        copy_dest_to_source(destination, source, fd.dest_size());
    else
        copy_source_to_dest(source, destination, filesize);
}

void SyncFile::delete_target_file(const wstring& source, const wstring& destination)
{
    log_info(translation::translate("target_deleted_source_missing"), destination,
        translation::translate("source_label"), source);
    const bool removed = ssh_dest() ? ssh_remove_file(destination) : delete_file(destination);  // [SyncFile.md#ssh-target-delete]
    if (!removed) {
        log_error(translation::translate("target_delete_failed"), destination);
        report_sync_error(translation::translate("target_delete_failed") + destination);
    }
}

void SyncFile::copy_source_to_dest(const wstring& source, const wstring& destination, uint64_t size)
{
    log_debug(L"copy_source_to_dest: " + source + L" -> " + destination);
    if (drive_not_exist(destination, ssh_dest()))
        return;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(translation::translate("copy_progress", {source, destination}), source, size);
    #endif
    try {  // [SyncFile.md#failures-return-early]
        pre_copy(source, destination);
        if (!save_copy_file(source, destination))
            return;
        if (ssh_ == SSHPATH::none)  // [SyncFile.md#local-only-filetime]
            util::set_last_write_time(destination, fs::last_write_time(source));
        log_info(translation::translate("copied_source_to_target"), source, translation::translate("to_label"), destination);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("copying_label"), source, destination, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(translation::translate("copying_label"), source, destination, translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_copy_error") + source + L" -> " + destination);
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
        Synchronize::set_progress_message(translation::translate("copy_progress", {destination, source}), destination, size);
    #endif
    try {
        pre_copy(destination, source);
        fs::file_time_type modify_time{};  // fs::last_write_time() only resolves a local path - see copy_source_to_dest()
        if (ssh_ == SSHPATH::none)
            modify_time = fs::last_write_time(destination);
        if (!save_copy_file(destination, source))
            return;
        if (ssh_ == SSHPATH::none)  // [SyncFile.md#local-only-filetime]
            set_last_write_time(source, modify_time);
        log_info(translation::translate("copied_target_to_source"), destination, translation::translate("to_label"), source);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("copying_label"), destination, source, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(translation::translate("copying_label"), destination, source, translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_copy_error") + destination + L" -> " + source);
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
        log_error(translation::translate("copying_label"), sync_item_.source(), sync_item_.target(), string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
    }
    catch (...) {
        log_error(translation::translate("copying_label"), sync_item_.source(), sync_item_.target(),
            L" - " + translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_error_for") + sync_item_.source());
    }
}

} // namespace AFS_Core
