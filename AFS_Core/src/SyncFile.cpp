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
#include <thread>
#include <vector>
#include "Global.h"
#include "Synchronize.h"
#include "StrUtil.h"
#include "FileTime.h"
#include "FileName.h"
#include "SystemInfo.h"
#include <chrono>
#include <FilesystemUtil.h>
#include <SymlinkUtil.h>
#include "FileDifference.h"
#include "Logging.h"
#include "Config.h"
#include "SyncFileSsh.h"
#include "CopyLog.h"
#include "Translation.h"

namespace AFS_Core {

namespace fs = std::filesystem;
using std::wstring;
using FDResult = FileDifference::Result;
using fsu::handle_last_error;
using util::string_to_wstring;
using util::to_string;
using fsu::file_exists;
using fsu::files_equal;

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
ReportedOnce both_modified;
ReportedOnce kept_files;
ReportedOnce dangling_links;

class OpenRights {  // [SyncFile.md#rights-catch-up]
    std::mutex mutex_;
    std::set<wstring> targets_;
public:
    void note(const wstring& target, bool open)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (open)
            targets_.insert(target);
        else
            targets_.erase(target);
    }
    bool open(const wstring& target)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return targets_.contains(target);
    }
};

OpenRights open_rights;

bool leads_nowhere(const wstring& path, bool ssh)  // [SyncFile.md#dangling-links]
{
    const bool dangling = !ssh && symlink_util::dangling_link(path);
    if (dangling_links.first_report(path, dangling))
        log_warning(translation::translate("symlink_dangling"), path);
    return dangling;
}

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

void report_both_modified(const wstring& source, const wstring& destination, bool conflict)  // [SyncFile.md#both-modified]
{
    if (!both_modified.first_report(destination, conflict))
        return;
    log_error(translation::translate("both_modified_not_copied"), source, L"<->", destination);
    report_sync_error(translation::translate("both_modified_not_copied") + L" " + source + L" <-> " + destination);
}

void report_kept_instead_of_deleted(const wstring& source, const wstring& destination, bool applies)  // [SyncFile.md#kept-instead-of-deleted]
{
    if (kept_files.first_report(destination, applies))
        log_warning(translation::translate("deleted_but_changed_kept"), source, L"<->", destination);
}
}

#ifdef _TEST
std::atomic<int> wait_for_updated_filetime_call_count{0};  // [SyncFile.md#wait_for_updated_filetime_call_count]
#endif

static wstring first_free_generation(const wstring& dest_filename, const auto& exists)  // [SyncFile.md#move_file_to_backup]
{
    for (int i = 1;; ++i) {
        const wstring candidate = dest_filename + FATHER_SON_SUFFIX + std::to_wstring(i);
        if (!exists(candidate))
            return candidate;
    }
}

static void move_file_to_backup(const wstring& dest_filename)  // [SyncFile.md#move_file_to_backup]
{
    log_debug(L"move_file_to_backup: " + dest_filename);
    const wstring new_dest_filename = first_free_generation(dest_filename, file_exists);
    try {
        fs::rename(dest_filename, new_dest_filename);
    }
    catch (const fs::filesystem_error& err) {
        throw SyncException(translation::translate("rename_error",
            {string_to_wstring(err.what()), dest_filename, new_dest_filename}));
    }
}

static void move_remote_file_to_backup(const wstring& dest_filename)  // [SyncFile.md#remote-father-son]
{
    log_debug(L"move_remote_file_to_backup: " + dest_filename);
    const std::size_t name_begin = dest_filename.find_last_of(L"/\\") + 1;
    std::set<wstring> siblings;
    for (const SshDirEntry& entry : ssh_list_dir(dest_filename.substr(0, name_begin)))
        siblings.insert(entry.name);
    if (!siblings.contains(dest_filename.substr(name_begin)))
        return;
    const wstring new_dest_filename = first_free_generation(dest_filename, [&](const wstring& candidate) { return siblings.contains(candidate.substr(name_begin)); });
    if (!ssh_rename(dest_filename, new_dest_filename))
        throw SyncException(translation::translate("rename_error",
            {translation::translate("remote_rename_error"), dest_filename, new_dest_filename}));
}

static void transfer_filename(const wstring& source, wstring& dest)
{
    log_debug(L"transfer_filename: " + source + L" -> " + dest);
    dest = util::with_trailing_char(dest, DIR_SEPARATOR) + fs::path(source).filename().wstring();
}

SyncFile::SyncFile(const SyncItem& si, std::size_t& copy_count, bool only_count, bool transfer_filename, SyncState* state, const wstring& state_root, const wstring& target_root)
    : SyncBase(si, copy_count, only_count, state, state_root, target_root)
    , transfer_filename_(transfer_filename)
{
    set_ssh();
}

void SyncFile::set_ssh()
{
    ssh_ = sync_item_.ssh_path_type();
}

void wait_for_updated_filetime(const fs::path& fn, const fs::file_time_type& modify_time);

fsu::FileInfoResult set_last_write_time(const fs::path& fn, const fs::file_time_type& modify_time, bool call_wait = true)  // [SyncFile.md#file-time-failure]
{
    log_debug(L"set_last_write_time " + fn.wstring() + L" filetime: " + string_to_wstring(util::to_string(modify_time)));
    fsu::FileInfoResult refusal = fsu::FileInfoResult::OK;
    for (int attempt = 0; attempt < 10; ++attempt) {  // [SyncFile.md#bounded-retries]
        refusal = fsu::write_time_refusal(fn.wstring(), modify_time);
        if (refusal == fsu::FileInfoResult::OK)
            break;
        if (Synchronize::stopping())
            return fsu::FileInfoResult::OK;
        log_debug(L"set_last_write_time: " + fsu::make_error_message(refusal));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (refusal == fsu::FileInfoResult::OK && call_wait)
        wait_for_updated_filetime(fn, modify_time);
    return refusal;
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
    while (!file_time::equal(fs::last_write_time(fn, ec), modify_time)) {
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

static CopyResult read_failed(const wstring& file, CopyFailure& failure)  // [SyncFile.md#copy-failure-reason]
{
    const std::error_code ec = fsu::last_stream_error();
    failure = {fsu::reason_for_failure(ec), fsu::failure_text(ec) + L": " + file};
    return CopyResult::Error;
}

static CopyResult write_failed(const wstring& file, CopyFailure& failure)  // [SyncFile.md#copy-failure-reason]
{
    const std::error_code ec = fsu::last_stream_error();
    failure = {fsu::reason_for_write_failure(ec), fsu::failure_text(ec) + L": " + file};
    return CopyResult::Error;
}

static CopyResult copy_local_file(const wstring& src, const wstring& dest, CopyFailure& failure)  // [SyncFile.md#copy_local_file]
{
    std::ifstream in(fs::path(src), std::ios::binary);
    if (!in)
        return read_failed(src, failure);
    std::ofstream out(fs::path(dest), std::ios::binary | std::ios::trunc);
    if (!out)
        return write_failed(dest, failure);
    std::vector<char> block(COPY_BLOCK_BYTES);
    uint64_t copied = 0;
    while (in.read(block.data(), static_cast<std::streamsize>(block.size())) || in.gcount() > 0) {
        if (Synchronize::stopping())
            return CopyResult::Stopped;
        if (!out.write(block.data(), in.gcount()))
            return write_failed(dest, failure);
        Synchronize::report_copied_bytes(copied += static_cast<uint64_t>(in.gcount()));
    }
    if (in.bad())
        return read_failed(src, failure);
    out.close();
    return out ? CopyResult::OK : write_failed(dest, failure);
}

static CopyResult copy_normal_file(const wstring& src, const wstring& dest, CopyFailure& failure)  // [SyncFile.md#copy-direction-follows-the-paths]
{
    const bool ssh_src = get_account_from_path(src).has_value();
    const bool ssh_dest = get_account_from_path(dest).has_value();
    if (ssh_src && ssh_dest)
        return copy_ssh_to_ssh(src, dest);
    if (ssh_src)
        return copy_ssh_to_fs(src, dest);
    if (ssh_dest)
        return copy_fs_to_ssh(src, dest);
    return copy_local_file(src, dest, failure);
}

CopyResult SyncFile::core_copy_function(const wstring& src, const wstring& dest)
{
    copy_failure_ = {};
    return copy_normal_file(src, dest, copy_failure_);
}

static void remove_partial_target(const wstring& dest)  // [SyncFile.md#restore_backup]
{
    if (!get_account_from_path(dest).has_value())
        fsu::remove_file(dest);
    else if (ssh_exists(dest))
        ssh_remove_file(dest);
}

static void restore_backup(const wstring& dest, const wstring& bak_filename)  // [SyncFile.md#restore_backup]
{
    remove_partial_target(dest);
    if (file_exists(bak_filename))
        fsu::rename_file(bak_filename, dest);
}

static wstring free_backup_name(const wstring& dest)  // [SyncFile.md#free_backup_name]
{
    wstring candidate = dest + L".bak";
    for (int i = 1; file_exists(candidate); ++i)
        candidate = dest + L".bak" + std::to_wstring(i);
    return candidate;
}

static void discard_backup(const wstring& bak_filename)  // [SyncFile.md#discard_backup]
{
    if (file_exists(bak_filename))
        fsu::remove_file(bak_filename);
}

bool SyncFile::refused(const wstring& file, fsu::FileInfoResult refusal) const  // [SyncFile.md#refused]
{
    if (refusal == fsu::FileInfoResult::OK)
        return false;
    const wstring message = fsu::make_error_message(refusal) + L": " + file;
    log_error(message);
    report_sync_error(message);
    return true;
}

bool SyncFile::time_refused(const wstring& file, fsu::FileInfoResult refusal)  // [SyncFile.md#file-time-failure]
{
    if (refusal == fsu::FileInfoResult::OK)
        return false;
    unstamped_.insert(file);
    const wstring message = translation::translate("file_time_not_settable") + file + L" (" + fsu::make_error_message(refusal) + L")";
    log_error(message);
    report_sync_error(message);
    return true;
}

static wstring attribute_message(const wstring& file, fsu::FileInfoResult refusal)  // [SyncFile.md#attribute-failure]
{
    const wstring what = fsu::make_error_message(fsu::FileInfoResult::CannotSetFileAttributes) + L": " + file;
    return refusal == fsu::FileInfoResult::CannotSetFileAttributes ? what : what + L" (" + fsu::make_error_message(refusal) + L")";
}

void SyncFile::carry_rights(const wstring& src, const wstring& dest) const  // [SyncFile.md#attribute-failure]
{
    std::error_code ec;
    const fs::perms rights = fs::status(src, ec).permissions();
    const fsu::FileInfoResult refusal = ec ? fsu::FileInfoResult::OK : fsu::permissions_refusal(dest, rights);
    open_rights.note(dest, refusal != fsu::FileInfoResult::OK);  // [SyncFile.md#rights-catch-up]
    if (refusal == fsu::FileInfoResult::OK)
        return;
    log_error(attribute_message(dest, refusal));
    report_sync_error(attribute_message(dest, refusal));
}

bool SyncFile::read_refused(const wstring& file) const  // [SyncFile.md#refused]
{
    return !get_account_from_path(file).has_value() && refused(file, fsu::read_refusal(file));
}

bool SyncFile::space_short(const wstring& file, uint64_t needed) const  // [SyncFile.md#disk-full]
{
    if (get_account_from_path(file).has_value())
        return false;
    const std::optional<uint64_t> available = fsu::free_space(fs::path(file).parent_path());
    if (!available || needed <= *available)
        return false;
    log_warning(fsu::make_error_message(fsu::FileInfoResult::DiskFull) + L": " + file);
    report_sync_shortage(file, needed, *available);
    return true;
}

bool SyncFile::copy_refused(const wstring& src, const wstring& dest, uint64_t size) const  // [SyncFile.md#refused]
{
    return read_refused(src) || space_short(dest, size);
}

void SyncFile::report_failed_copy(const wstring& src, const wstring& dest) const  // [SyncFile.md#copy-failure-reason]
{
    log_error(translation::translate("copying_label"), src, dest, translation::translate("copy_failed") + L" " + copy_failure_.text);
    if (copy_failure_.reason == fsu::FileInfoResult::DiskFull) {
        std::error_code ec;
        report_sync_shortage(dest, fs::file_size(src, ec), fsu::free_space(fs::path(dest).parent_path()).value_or(0));
        return;
    }
    report_sync_error(translation::translate("copy_failed_for") + src + L" -> " + dest + (copy_failure_.text.empty() ? wstring() : L" (" + copy_failure_.text + L")"));
}

bool SyncFile::save_copy_file(const wstring& src, const wstring& dest)
{
    log_debug(L"save_copy_file");
    const wstring bak_filename = free_backup_name(dest);
    if (file_exists(dest) && refused(dest, fsu::rename_refusal(dest, bak_filename)))  // [SyncFile.md#backup-rename-failure]
        return false;
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
        report_failed_copy(src, dest);
        restore_backup(dest, bak_filename);
        return false;
    }
    if (copy_ok == CopyResult::Stopped) {
        log_info(translation::translate("copy_cancelled"), src);
        restore_backup(dest, bak_filename);
        return false;
    }
    carry_rights(src, dest);
    discard_backup(bak_filename);
    return true;
}

void SyncFile::pre_copy(const wstring& dest)  // [SyncFile.md#pre_copy]
{
    if (get_account_from_path(dest).has_value()) {
        if (sync_item_.father_son_principle())
            move_remote_file_to_backup(dest);
        return;
    }
    if (!file_exists(dest))
        return;
    const fsu::FileInfoResult refusal = fsu::permissions_refusal(dest, fs::perms::owner_all);
    if (refusal != fsu::FileInfoResult::OK)
        log_error(attribute_message(dest, refusal));
    if (sync_item_.father_son_principle())
        move_file_to_backup(dest);
}

std::optional<PlannedAction> SyncFile::decide(const FileDifference& fd) const  // [SyncFile.md#decide]
{
    const FDResult fdiff = fd.result();
    if (fdiff == FDResult::SourceDeleted || (fdiff == FDResult::SourceNotExits && sync_item_.delete_in_target()))
        return PlannedAction::DeleteInTarget;
    if (fdiff == FDResult::DestDeleted || (fdiff == FDResult::Equal && completes_moves()))  // [SyncFile.md#state] [SyncFile.md#move-catch-up]
        return PlannedAction::DeleteInSource;
    const bool copy_to_target = fdiff == FDResult::DestNotExists || fdiff == FDResult::SourceModified
        || (fdiff == FDResult::DestModified && sync_item_.copy_when_target_newer());
    if (copy_to_target)
        return sync_item_.del_aftercopy() && !ssh_source() ? PlannedAction::MoveToTarget : PlannedAction::CopyToTarget;
    if (sync_item_.two_way_sync() && (fdiff == FDResult::SourceNotExits || fdiff == FDResult::DestModified))
        return PlannedAction::CopyToSource;
    if (sync_item_.two_way_sync() && sync_item_.conflict_copies() && (fdiff == FDResult::BothModified || fdiff == FDResult::ContentConflict))  // [SyncFile.md#conflict-copies]
        return fd.dest_newer() ? PlannedAction::ConflictCopyToSource : PlannedAction::ConflictCopyToTarget;
    return std::nullopt;
}

bool SyncFile::empty_file_skipped(const FileDifference& fd, PlannedAction action) const  // [SyncFile.md#empty-source-file]
{
    if (sync_item_.sync_empty_files() || deletion(action))
        return false;
    return (winner_in_target(action) ? fd.dest_size() : fd.source_size()) == 0;
}

bool SyncFile::not_readable(const FileDifference& fd) const  // [SyncFile.md#not-readable]
{
    if (fd.result() != FDResult::SourceNotReadable && fd.result() != FDResult::DestNotReadable)
        return false;
    log_error(fd.read_error());
    report_sync_error(fd.read_error());
    return true;
}

bool SyncFile::only_time_differs(const FileDifference& fd, const wstring& source, const wstring& destination) const  // [SyncFile.md#restamp]
{
    const bool one_side_changed = fd.result() == FDResult::DestModified || (fd.result() == FDResult::SourceModified && fd.has_base());
    return ssh_ == SSHPATH::none && one_side_changed && fd.source_size() == fd.dest_size() && files_equal(source, destination);
}

bool SyncFile::restamped(const FileDifference& fd, const wstring& source, const wstring& destination)  // [SyncFile.md#restamp]
{
    if (!only_time_differs(fd, source, destination))
        return false;
    if (only_count_)
        return true;
    const bool target_stale = fd.result() == FDResult::DestModified;
    const wstring& stale = target_stale ? destination : source;
    if (time_refused(stale, fsu::write_time_refusal(stale, target_stale ? fd.source_time() : fd.dest_time())))
        return true;
    log_info(translation::translate("file_time_adjusted"), stale);
    record_pair(state_key_, source, destination);
    return true;
}

void SyncFile::sync_difference(const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    if (not_readable(fd) || restamped(fd, source, destination))
        return;
    const std::optional<PlannedAction> action = decide(fd);
    if (!action) {
        report_newer_target(source, destination, fd.result() == FDResult::DestModified);
        report_content_conflict(source, destination, fd.result() == FDResult::ContentConflict);
        report_both_modified(source, destination, fd.result() == FDResult::BothModified);
        settle_state(fd);
        if (fd.result() == FDResult::Equal && !only_count_ && ssh_ == SSHPATH::none && open_rights.open(destination))  // [SyncFile.md#rights-catch-up]
            carry_rights(source, destination);
        return;
    }
    const bool kept = fd.has_base() && (fd.result() == FDResult::SourceNotExits || fd.result() == FDResult::DestNotExists);
    report_kept_instead_of_deleted(source, destination, kept);
    if (rename_instead(*action, fd, source, destination) || defer_deletion(*action, fd, source, destination))
        return;
    if (only_count_) {  // [SyncFile.md#only-count-touches-nothing]
        if (empty_file_skipped(fd, *action))
            return;
        ++copy_count_;
        plan_step({sync_item_.name(), *action, source, destination, file_in_target(*action) ? fd.dest_size() : fd.source_size()});
        return;
    }
    if (kept && !ensure_parent_dir(*action, source, destination))
        return;
    carry_out(*action, fd, source, destination, filesize);
}

void SyncFile::carry_out(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    if (empty_file_skipped(fd, action)) {
        log_warning(translation::translate(action == PlannedAction::CopyToSource ? "empty_target_not_copied" : "empty_source_not_copied"),
            action == PlannedAction::CopyToSource ? destination : source);
        return;
    }
    if (perform(action, fd, source, destination, filesize))
        update_state(action, source, destination);
}

bool SyncFile::ensure_parent_dir(PlannedAction action, const wstring& source, const wstring& destination)  // [SyncFile.md#parent-of-a-kept-file]
{
    const bool into_source = action == PlannedAction::CopyToSource;
    return create_parent_dir(into_source ? source : destination, into_source ? ssh_source() : ssh_dest(), into_source);
}

bool SyncFile::create_parent_dir(const wstring& path, bool ssh, bool in_source)
{
    const wstring parent = path.substr(0, path.find_last_of(L"/\\"));
    const char* message_id = in_source ? "source_dir_create_error" : "target_dir_create_error";
    if (!ssh)
        return local_dir_created(parent, message_id);
    if (ssh_mkdir_p(parent))
        return true;
    log_error(translation::translate(message_id), parent);
    report_sync_error(translation::translate(message_id) + parent);
    return false;
}

bool SyncFile::defer_deletion(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination)  // [SyncFile.md#deferred-deletions]
{
    if (state_ == nullptr || !deletion(action) || sync_item_.sync_item_type() == SyncItemType::File)
        return false;
    state_->defer({state_key_, source, destination, action == PlannedAction::DeleteInTarget, file_in_target(action) ? fd.dest_size() : fd.source_size()});
    return true;
}

void SyncFile::carry_out_deferred(const PendingDeletion& deletion)  // [SyncFile.md#deferred-deletions]
{
    state_key_ = deletion.key;
    const PlannedAction action = deletion.in_target ? PlannedAction::DeleteInTarget : PlannedAction::DeleteInSource;
    if (only_count_) {
        ++copy_count_;
        plan_step({sync_item_.name(), action, deletion.source, deletion.target, deletion.bytes, deletion.link});
        return;
    }
    const bool removed = deletion.in_target ? delete_target_file(deletion.source, deletion.target) : delete_source_file(deletion.source, deletion.target);
    if (removed)
        update_state(action, deletion.source, deletion.target);
}

std::optional<wstring> SyncFile::rename_candidate(const FileDifference& fd, bool new_in_source) const  // [SyncFile.md#renames]
{
    const uint64_t size = new_in_source ? fd.source_size() : fd.dest_size();
    const fs::file_time_type time = new_in_source ? fd.source_time() : fd.dest_time();
    std::optional<wstring> candidate;
    for (const wstring& key : state_->with_size(size)) {
        const FileState& entry = *state_->find(key);
        if (key == state_key_ || !fd.same_time(time, new_in_source ? entry.source_time : entry.target_time))
            continue;
        if (candidate)  // two files alike: which of them moved cannot be told, so neither is renamed
            return std::nullopt;
        candidate = key;
    }
    return candidate;
}

bool SyncFile::old_file_moved_away(const FileDifference& fd, const wstring& old_key, bool new_in_source, bool pending) const
{
    if (pending)  // the pass already found it gone on this side and unchanged on the other
        return true;
    const FileState& entry = *state_->find(old_key);
    if (observe_file(side_path(old_key, !new_in_source), new_in_source ? ssh_source() : ssh_dest()))  // still there: a copy, not a move
        return false;
    const std::optional<ObservedFile> other = observe_file(side_path(old_key, new_in_source), new_in_source ? ssh_dest() : ssh_source());
    return other && other->size == entry.size && fd.same_time(other->time, new_in_source ? entry.target_time : entry.source_time);
}

bool SyncFile::rename_path(const wstring& from, const wstring& to, bool ssh, bool in_source)
{
    if (!create_parent_dir(to, ssh, in_source))
        return false;
    if (ssh)
        return ssh_rename(from, to);
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec)
        log_error(translation::translate("rename_failed"), from, to, string_to_wstring(ec.message()));
    return !ec;
}

bool SyncFile::rename_instead(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination)  // [SyncFile.md#renames]
{
    const bool new_in_source = action == PlannedAction::CopyToTarget;
    if (state_ == nullptr || state_->pass_only() || !(new_in_source || action == PlannedAction::CopyToSource) || state_->find(state_key_) != nullptr  // [SyncFile.md#renames]
        || fd.result() != (new_in_source ? FDResult::DestNotExists : FDResult::SourceNotExits)
        || (new_in_source && !sync_item_.two_way_sync() && !sync_item_.delete_in_target()))  // without deletions the old target stays, as asked
        return false;
    const std::optional<wstring> old_key = rename_candidate(fd, new_in_source);
    if (!old_key)
        return false;
    std::optional<PendingDeletion> pending = state_->claim(*old_key);
    const wstring old_path = side_path(*old_key, new_in_source);
    const wstring& new_path = new_in_source ? destination : source;
    const bool renamed = old_file_moved_away(fd, *old_key, new_in_source, pending.has_value())
        && (only_count_ || rename_path(old_path, new_path, new_in_source ? ssh_dest() : ssh_source(), !new_in_source));
    if (!renamed) {
        if (pending)
            state_->defer(std::move(*pending));
        return false;
    }
    ++copy_count_;
    state_->renamed(*old_key);
    if (only_count_) {
        plan_step({sync_item_.name(), new_in_source ? PlannedAction::RenameInTarget : PlannedAction::RenameInSource, old_path, new_path, 0});
        return true;
    }
    log_info(translation::translate(new_in_source ? "renamed_in_target" : "renamed_in_source"), old_path, L"->", new_path);
    state_->forget(*old_key);
    update_state(PlannedAction::CopyToTarget, source, destination);
    return true;
}

bool SyncFile::perform(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination, uint64_t filesize)
{
    switch (action) {
    case PlannedAction::DeleteInTarget: return delete_target_file(source, destination);
    case PlannedAction::DeleteInSource: return delete_source_file(source, destination);
    case PlannedAction::CopyToSource: return copy_dest_to_source(destination, source, fd.dest_size());  // [SyncFile.md#volume-is-target-size]
    case PlannedAction::ConflictCopyToTarget:
    case PlannedAction::ConflictCopyToSource: return resolve_conflict(action, fd, source, destination);
    case PlannedAction::RenameInTarget:
    case PlannedAction::RenameInSource:  // carried out by rename_instead() before anything reaches here
    case PlannedAction::CopyToTarget:
    case PlannedAction::MoveToTarget: break;
    }
    return copy_source_to_dest(source, destination, filesize);
}

void SyncFile::update_state(PlannedAction action, const wstring& source, const wstring& destination)  // [SyncFile.md#state]
{
    if (state_ == nullptr)
        return;
    if (deletion(action) || action == PlannedAction::MoveToTarget) {
        state_->forget(state_key_);
        return;
    }
    record_pair(state_key_, source, destination);
}

void SyncFile::record_pair(const wstring& key, const wstring& source, const wstring& destination)
{
    std::optional<ObservedFile> observed_source = observe_file(source, ssh_source());
    std::optional<ObservedFile> observed_dest = observe_file(destination, ssh_dest());
    if (state_ == nullptr || !observed_source || !observed_dest)
        return;
    if (unstamped_.contains(destination))  // [SyncFile.md#restamp]
        observed_dest->time = observed_source->time;
    if (unstamped_.contains(source))
        observed_source->time = observed_dest->time;
    state_->record(key, {observed_source->size, observed_source->time, observed_dest->time});
}

wstring conflict_copy_name(const wstring& path, bool ssh)  // [SyncFile.md#conflict-copy-name]
{
    const std::size_t slash = path.find_last_of(L"/\\") + 1;
    const wstring dir = path.substr(0, slash), file = path.substr(slash);
    const std::size_t dot = file.rfind(L'.');
    const bool has_extension = dot != wstring::npos && dot != 0;
    const wstring stem = has_extension ? file.substr(0, dot) : file, extension = has_extension ? file.substr(dot) : wstring();
    const std::optional<SshAccount> account = ssh ? get_account_from_path(path) : std::nullopt;
    const wstring host = string_to_wstring(account ? account->host() : system_info::host_name());
    wstring marker = translation::translate("conflict_copy_marker", {file_name::local_date(), host});
    if (marker == L"conflict_copy_marker")  // [SyncFile.md#conflict-copy-name] no catalogue bound: the key must not become a file name
        marker = L"conflict " + file_name::local_date() + L", " + host;
    for (int n = 1;; ++n) {
        const wstring candidate = dir + stem + L" (" + marker + (n == 1 ? wstring() : L" " + std::to_wstring(n)) + L")" + extension;
        if (!observe_file(candidate, ssh))
            return candidate;
    }
}

bool SyncFile::resolve_conflict(PlannedAction action, const FileDifference& fd, const wstring& source, const wstring& destination)  // [SyncFile.md#conflict-copies]
{
    const bool source_wins = action == PlannedAction::ConflictCopyToTarget;
    const wstring& loser = source_wins ? destination : source;
    const bool loser_ssh = source_wins ? ssh_dest() : ssh_source();
    const wstring& winner = source_wins ? source : destination;
    if (read_refused(winner))  // [SyncFile.md#refused]
        return false;
    const wstring copy = conflict_copy_name(loser, loser_ssh);
    if (!rename_path(loser, copy, loser_ssh, !source_wins))
        return false;
    const wstring copy_name = copy.substr(copy.find_last_of(L"/\\") + 1);
    const wstring twin = winner.substr(0, winner.find_last_of(L"/\\") + 1) + copy_name;  // the conflict copy's place on the winning side
    log_warning(translation::translate("conflict_copy_made", {loser, copy_name}));
    Synchronize::note(LogLevel::Warning, translation::translate("conflict_copy_made", {loser, copy_name}));  // [Synchronize.md#notes]
    const bool settled = source_wins ? copy_source_to_dest(source, destination, fd.source_size()) : copy_dest_to_source(destination, source, fd.dest_size());
    const bool travelled = source_wins ? copy_dest_to_source(copy, twin, fd.dest_size()) : copy_source_to_dest(copy, twin, fd.source_size());
    if (travelled)
        record_pair(state_key(source_wins ? twin : copy), source_wins ? twin : copy, source_wins ? copy : twin);
    return settled;
}

void SyncFile::settle_state(const FileDifference& fd)  // [SyncFile.md#state]
{
    if (state_ == nullptr || only_count_)
        return;
    if (fd.result() == FDResult::Equal)
        state_->record(state_key_, {fd.source_size(), fd.source_time(), fd.dest_time()});
    else if (fd.result() == FDResult::NeitherExists)
        state_->forget(state_key_);
}

bool SyncFile::delete_target_file(const wstring& source, const wstring& destination)
{
    log_info(translation::translate("target_deleted_source_missing"), destination,
        translation::translate("source_label"), source);
    return deleted(destination, ssh_dest(), "target_delete_failed");  // [SyncFile.md#ssh-target-delete]
}

bool SyncFile::delete_source_file(const wstring& source, const wstring& destination)  // [SyncFile.md#state]
{
    log_info(translation::translate(completes_moves() ? "source_deleted_already_in_target" : "source_deleted_target_missing"), source,
        translation::translate("target_label"), destination);
    return deleted(source, ssh_source(), "source_delete_failed");
}

bool SyncFile::copy_source_to_dest(const wstring& source, const wstring& destination, uint64_t size)
{
    log_debug(L"copy_source_to_dest: " + source + L" -> " + destination);
    if (drive_not_exist(destination, ssh_dest()))
        return false;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(translation::translate("copy_progress", {source, destination}), source, size);
    #endif
    if (copy_refused(source, destination, size))
        return false;
    try {  // [SyncFile.md#failures-return-early]
        pre_copy(destination);
        if (!save_copy_file(source, destination))
            return false;
        if (ssh_ == SSHPATH::none)  // [SyncFile.md#local-only-filetime]
            time_refused(destination, fsu::write_time_refusal(destination, fs::last_write_time(source)));
        log_info(translation::translate("copied_source_to_target"), source, translation::translate("to_label"), destination);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return false;
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("copying_label"), source, destination, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return false;
    }
    catch (...) {
        log_error(translation::translate("copying_label"), source, destination, translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_copy_error") + source + L" -> " + destination);
        return false;
    }
    ++copy_count_;
    report_sync_volume(size);
    if (sync_item_.del_aftercopy() && !ssh_source())  // an SSH source cannot be deleted through the local filesystem
        deleted(source, false, "source_delete_failed");  // [SyncFile.md#move-keeps-the-copy]
    return true;
}

bool SyncFile::copy_dest_to_source(const wstring& destination, const wstring& source, uint64_t size)
{
    log_debug(L"copy_dest_to_source: " + destination + L" -> " + source);
    if (drive_not_exist(source, ssh_source()))
        return false;
    #ifdef _WXWIDGETS
        Synchronize::set_progress_message(translation::translate("copy_progress", {destination, source}), destination, size);
    #endif
    if (copy_refused(destination, source, size))
        return false;
    try {
        pre_copy(source);
        fs::file_time_type modify_time{};  // fs::last_write_time() only resolves a local path - see copy_source_to_dest()
        if (ssh_ == SSHPATH::none)
            modify_time = fs::last_write_time(destination);
        if (!save_copy_file(destination, source))
            return false;
        if (ssh_ == SSHPATH::none)  // [SyncFile.md#local-only-filetime]
            time_refused(source, set_last_write_time(source, modify_time));
        log_info(translation::translate("copied_target_to_source"), destination, translation::translate("to_label"), source);
    }
    catch (const fs::filesystem_error& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return false;
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("copying_label"), destination, source, string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return false;
    }
    catch (...) {
        log_error(translation::translate("copying_label"), destination, source, translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_copy_error") + destination + L" -> " + source);
        return false;
    }
    ++copy_count_;
    report_sync_volume(size);
    return true;
}

const FileState* SyncFile::file_base() const  // [SyncFile.md#file-base]
{
    const FileState* known = state_ != nullptr ? state_->find(state_key_) : nullptr;
    return known != nullptr && known->link ? nullptr : known;
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
        state_key_ = state_key(source);  // [SyncFile.md#state]
        if (state_ != nullptr)
            state_->seen(state_key_);
        if (leads_nowhere(source, ssh_source()) || leads_nowhere(destination, ssh_dest()))
            return;
        FileDifference fd(source, destination, sync_item_.compare_items(), ssh_, sync_item_.two_way_sync(), file_base());
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
