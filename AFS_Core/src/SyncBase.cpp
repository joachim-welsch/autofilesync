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
#include "SyncBase.h"
#include <FilesystemUtil.h>
#include "CopyLog.h"
#include "FileDelete.h"
#include "Global.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "SyncFile.h"
#include "SyncFileSsh.h"
#include "SyncLink.h"
#include <SymlinkUtil.h>
#include "SyncPlan.h"
#include "Synchronize.h"
#include "Translation.h"
#include "StrUtil.h"
#include <algorithm>
#include <mutex>
#include <filesystem>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

namespace {
    std::mutex refused_mutex;
    std::set<wstring> refused_items;  // [SyncBase.md#mass-deletion]

    bool first_refusal(const wstring& item, bool refused)
    {
        std::lock_guard<std::mutex> lock(refused_mutex);
        if (refused)
            return refused_items.insert(item).second;
        refused_items.erase(item);
        return false;
    }

    wstring root_of(const SyncItem& item)  // the item's source as SyncFile::operator()() sees its files
    {
        return item.ssh_source() ? item.source() : without_volume_spec(item.source());
    }

    wstring target_root_of(const SyncItem& item)
    {
        return item.ssh_target() ? item.target() : without_volume_spec(item.target());
    }

    bool below(const wstring& path, const wstring& root)
    {
        if (!path.starts_with(root))
            return false;
        return path.size() == root.size() || path[root.size()] == L'/' || path[root.size()] == L'\\' || root.ends_with(L'/') || root.ends_with(L'\\');
    }
}

SyncBase::SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count, SyncState* state, const wstring& state_root, const wstring& target_root)
    : sync_item_(sync_item), copy_count_(copy_count), only_count_(only_count)
    , source_kept_(sync_item.del_aftercopy() && !sync_item.ssh_source() && fsu::read_only_filesystem(without_volume_spec(sync_item.source())))
    , moves_(sync_item.del_aftercopy() && !sync_item.ssh_source() && !source_kept_), state_(state)
    , state_root_(state_root.empty() ? root_of(sync_item) : state_root)
    , target_root_(target_root.empty() ? target_root_of(sync_item) : target_root)
{}

wstring SyncBase::side_path(const wstring& key, bool in_target) const  // [SyncBase.md#side_path]
{
    const wchar_t separator = (in_target ? sync_item_.ssh_target() : sync_item_.ssh_source()) ? L'/' : DIR_SEPARATOR;
    wstring relative = key;
    std::replace(relative.begin(), relative.end(), L'/', separator);
    return with_trailing_char(in_target ? target_root_ : state_root_, separator) + relative;
}

bool mass_deletion(std::size_t deletions, std::size_t known_files)  // [SyncBase.md#mass-deletion]
{
    return deletions >= MASS_DELETION_MIN_FILES && deletions * 100 > known_files * MASS_DELETION_PERCENT;
}

bool SyncBase::deletions_refused(const std::vector<PendingDeletion>& deletions)  // [SyncBase.md#mass-deletion]
{
    const std::size_t known = state_->files().size()
        + std::ranges::count_if(deletions, [this](const PendingDeletion& deletion) { return state_->find(deletion.key) == nullptr; });  // [SyncBase.md#known-files]
    const bool exceeded = mass_deletion(deletions.size(), known) && !state_->mass_deletion_approved();
    const std::initializer_list<wstring> values{sync_item_.name(), std::to_wstring(deletions.size()), std::to_wstring(known)};
    if (exceeded && only_count_) {
        log_warning(translation::translate("mass_deletion_planned", values));
        Synchronize::note(LogLevel::Warning, translation::translate("mass_deletion_planned", values));  // [Synchronize.md#notes]
    }
    if (!first_refusal(sync_item_.name(), exceeded && !only_count_))
        return exceeded && !only_count_;
    log_error(translation::translate("mass_deletion_refused", values));
    report_sync_error(translation::translate("mass_deletion_refused", values));
    Synchronize::note(LogLevel::Error, translation::translate("mass_deletion_refused", values));
    return true;
}

void SyncBase::finish_pass()  // [SyncBase.md#finish_pass]
{
    carry_out_deferred();
    report_links();
    report_mode_conflict();
}

void SyncBase::report_link_count(const LinkTally::Count& count, const char* message_id) const  // [SyncBase.md#report_links]
{
    if (!first_refusal(sync_item_.name() + L"|" + string_to_wstring(message_id), count.links > 0))
        return;
    const wstring detail = std::to_wstring(count.links) + L" (" + count.first + L")";
    log_warning(translation::translate(message_id), detail);
    Synchronize::note(LogLevel::Warning, translation::translate(message_id) + L" " + detail);
}

void SyncBase::report_links()  // [SyncBase.md#report_links]
{
    if (only_count_)
        return;
    LinkTally::Count unresolved;
    for (const wstring& link : link_tally_.created)
        if (symlink_util::dangling_link(link))
            unresolved.add(link);
    report_link_count(link_tally_.unsupported, "links_not_supported_skipped");
    report_link_count(link_tally_.system, "system_links_skipped");
    report_link_count(link_tally_.outside, "links_leave_tree");
    report_link_count(unresolved, "links_unresolved_in_target");
    report_link_count(link_tally_.loops, "links_loop_skipped");
    report_link_count(link_tally_.into_target, "links_into_target_skipped");
    report_link_count(link_tally_.too_deep, "links_too_deep_skipped");
    report_link_count(link_tally_.dangling, "links_dangling_skipped");
    report_link_count(link_tally_.junctions, "junctions_skipped");
    link_tally_ = {};
}

bool SyncBase::links_as_links() const  // [SyncBase.md#links_as_links]
{
    return sync_item_.symlink_mode() == SymlinkMode::AsLink;
}

bool SyncBase::follows_links() const  // [SyncBase.md#follows_links]
{
    return sync_item_.symlink_mode() == SymlinkMode::Follow && !sync_item_.two_way_sync() && !sync_item_.del_aftercopy();
}

void SyncBase::report_mode_conflict() const  // [SyncBase.md#follows_links]
{
    const bool conflict = sync_item_.symlink_mode() == SymlinkMode::Follow && (sync_item_.two_way_sync() || sync_item_.del_aftercopy());
    if (only_count_ || !first_refusal(sync_item_.name() + L"|symlink_follow_needs_one_way", conflict))
        return;
    log_warning(translation::translate("symlink_follow_needs_one_way"), sync_item_.name());
    Synchronize::note(LogLevel::Warning, translation::translate("symlink_follow_needs_one_way") + L" " + sync_item_.name());
}

bool SyncBase::target_is_link(const wstring& target)  // [SyncBase.md#target_usable]
{
    std::error_code ec;
    if (!sync_item_.ssh_target())
        return fs::is_symlink(fs::symlink_status(target, ec));
    if (!ssh_keeps_links(target))
        return false;
    const std::size_t slash = target.find_last_of(L'/');
    const wstring dir = target.substr(0, slash);
    auto listed = remote_target_links_.find(dir);
    if (listed == remote_target_links_.end()) {
        listed = remote_target_links_.emplace(dir, std::set<wstring>()).first;
        for (const SshDirEntry& entry : ssh_list_dir(dir))
            if (entry.is_symlink)
                listed->second.insert(entry.name);
    }
    return listed->second.contains(target.substr(slash + 1));
}

bool SyncBase::link_removed(const wstring& target)
{
    std::error_code ec;
    if (!sync_item_.ssh_target())
        return fs::remove(target, ec);
    const std::size_t slash = target.find_last_of(L'/');
    remote_target_links_[target.substr(0, slash)].erase(target.substr(slash + 1));
    return ssh_remove_file(target);
}

bool SyncBase::link_filtered(const wstring& name, const wstring& path, bool ssh, bool directory, const wstring& wildcard) const  // [SyncBase.md#link_filtered]
{
    if (is_excluded(name, sync_item_.exclusions()))
        return true;
    if (wildcard.empty() || directory || wildmatch(name, wildcard))
        return false;
    const std::optional<SshLink> link = ssh ? ssh_link_info(path) : std::nullopt;
    return !(link && link->to_directory);
}

bool SyncBase::target_usable(const wstring& target)  // [SyncBase.md#target_usable]
{
    if ((!links_as_links() && !follows_links()) || only_count_ || !target_is_link(target))
        return true;
    if (sync_item_.delete_in_target() && link_removed(target)) {
        log_info(translation::translate("target_link_replaced"), target);
        return true;
    }
    log_error(translation::translate("target_is_link"), target);
    report_sync_error(translation::translate("target_is_link") + target);
    return false;
}

bool SyncBase::system_link_skipped(const wstring& path)  // [SyncBase.md#system_link_skipped]
{
    if (sync_item_.ssh_source() || !symlink_util::system_link(path))
        return false;
    link_tally_.system.add(path);
    return true;
}

void SyncBase::carry_out_deferred()  // [SyncBase.md#finish_pass]
{
    if (state_ == nullptr)
        return;
    const std::vector<PendingDeletion> deletions = state_->take_deletions();
    const std::vector<PendingDirRemoval> removals = state_->take_dir_removals();
    if (deletions_refused(deletions))
        return;
    for (const PendingDeletion& deletion : deletions) {
        if (stop_requested())
            return;
        SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), deletion.source, deletion.target, sync_item_.wildcard(), Exclusions(),
                          sync_item_.flags(), sync_item_.compare_items()),
                 copy_count_, only_count_, false, state_, state_root_, target_root_).carry_out_deferred(deletion);
    }
    for (const PendingDirRemoval& removal : removals)
        if (!stop_requested())
            remove_dir_if_empty(removal.path, removal.ssh);
}

wstring SyncBase::state_key(const wstring& source) const  // [SyncBase.md#state_key]
{
    wstring key = below(source, state_root_) ? source.substr(state_root_.size()) : source;
    const std::size_t start = key.find_first_not_of(L"/\\");
    key = start == wstring::npos ? wstring() : key.substr(start);
    if (key.empty())
        key = source.substr(source.find_last_of(L"/\\") + 1);
    std::replace(key.begin(), key.end(), L'\\', L'/');
    return key;
}

bool SyncBase::drive_not_exist(const wstring& pathname, bool ssh)
{
    log_debug(L"drive_not_exist: " + pathname);
    if (ssh)
        return false; // no mount check for SSH paths yet: the copy itself reports an unreachable host
    const bool retval = !MountedDrives::instance().drive_mounted(pathname);
    if (retval)
        log_debug(L"drive_not_exist: TRUE");
    return retval;
}

bool SyncBase::local_dir_created(const wstring& dir, const char* message_id)  // [SyncBase.md#local_dir_created]
{
    std::error_code ec;
    if (fsu::create_directory_refusal(dir, ec) == fsu::FileInfoResult::OK)
        return true;
    const wstring message = translation::translate(message_id) + dir + L": " + fsu::failure_text(ec);
    log_error(message);
    report_sync_refusal(message, fsu::reason_for_failure(ec));
    return false;
}

static wstring known_reason(fsu::FileInfoResult refusal)  // [SyncBase.md#deleted]
{
    return refusal == fsu::FileInfoResult::ErrorWriteFileInfo ? wstring() : L" (" + fsu::make_error_message(refusal) + L")";
}

static void report_remote_dir(const char* message_id, const wstring& dir)  // [SyncBase.md#report_remote_dir]
{
    const fsu::FileInfoResult told = ssh_take_failure_reason();  // [SyncFileSsh.md#failure-reason]
    const bool known = told != fsu::FileInfoResult::OK && told != fsu::FileInfoResult::UnknownError;
    const fsu::FileInfoResult refusal = known ? told : ssh_read_only_filesystem(dir) ? fsu::FileInfoResult::ReadOnlyFilesystem : fsu::FileInfoResult::ErrorWriteFileInfo;
    report_sync_refusal(translation::translate(message_id) + dir + known_reason(refusal), refusal);
}

void SyncBase::report_failure(const wstring& where, const fs::filesystem_error& failure)  // [SyncBase.md#report_failure]
{
    const wstring message = fsu::failure_text(failure.code()) + L": " + (failure.path1().empty() ? where : failure.path1().wstring());
    log_error(message);
    report_sync_refusal(message, fsu::reason_for_failure(failure.code()));
}

bool SyncBase::create_target_dir_if_not_exist(const std::wstring& targetdir)  // [SyncBase.md#create_target_dir_if_not_exist]
{
    log_debug(L"create_target_dir_if_not_exist: " + targetdir);
    if (only_count_)
        return true;
    const wstring destination = targetdir.empty() ? sync_item_.target() : targetdir;
    if (drive_not_exist(destination, sync_item_.ssh_target()))
        return true;
    if (!sync_item_.ssh_target())
        return local_dir_created(without_volume_spec(destination), "target_dir_not_creatable");
    ssh_take_failure_reason();  // [SyncFileSsh.md#failure-reason] what an earlier call left is not this directory's
    if (ssh_mkdir_p(destination))
        return true;
    log_error(translation::translate("ssh_dir_create_error") + destination);
    report_remote_dir("ssh_target_dir_not_creatable", destination);
    return false;
}

bool SyncBase::create_source_dir(const wstring& dir)  // [SyncBase.md#create_source_dir]
{
    log_debug(L"create_source_dir: " + dir);
    if (only_count_)
        return true;
    if (!sync_item_.ssh_source())
        return local_dir_created(dir, "source_dir_create_error");
    ssh_take_failure_reason();
    if (ssh_mkdir_p(dir))
        return true;
    log_error(translation::translate("source_dir_create_error"), dir);
    report_remote_dir("source_dir_create_error", dir);
    return false;
}

void SyncBase::remove_dir_if_empty(const wstring& dir, bool ssh)  // [SyncBase.md#remove_dir_if_empty]
{
    if (only_count_)
        return;
    std::error_code ec;
    if (ssh ? !ssh_list_dir(dir).empty() : !fs::is_empty(dir, ec) || ec)
        return;
    const fsu::FileInfoResult refusal = ssh ? ssh_rmdir_refusal(dir) : fsu::remove_refusal(dir);
    if (refusal == fsu::FileInfoResult::OK)
        log_info(translation::translate("empty_dir_removed"), dir);
    if (refusal == fsu::FileInfoResult::OK || refusal == fsu::FileInfoResult::FileNotExist)  // [SyncBase.md#already-gone]
        return;
    const wstring message = translation::translate("dir_delete_failed") + dir + known_reason(refusal);
    log_error(message);
    report_sync_refusal(message, refusal);
}

bool SyncBase::deleted(const wstring& file, bool ssh, const char* message_id)  // [SyncBase.md#deleted]
{
    const fsu::FileInfoResult refusal = ssh ? ssh_remove_refusal(file) : delete_refusal(file);
    if (refusal == fsu::FileInfoResult::OK)
        return true;
    if (refusal == fsu::FileInfoResult::FileNotExist) {  // [SyncBase.md#already-gone]
        log_debug(L"deleted: already gone: " + file);
        return true;
    }
    const wstring message = translation::translate(message_id) + file + known_reason(refusal);
    log_error(message);
    report_sync_refusal(message, refusal);
    return false;
}

bool SyncBase::completes_moves() const  // [SyncBase.md#completes_moves]
{
    return moves_ && !sync_item_.two_way_sync() && !sync_item_.delete_in_target();
}

void SyncBase::guarded(const wstring& source, const wstring& dest, const std::function<void()>& work)  // [SyncBase.md#sync_file]
{
    try {
        work();
    } catch (const fs::filesystem_error& e) {
        report_failure(source, e);
    } catch (const std::exception& e) {
        log_error(source, dest, string_to_wstring(e.what()));
        report_sync_error(source + L": " + string_to_wstring(e.what()));
    } catch (...) {
        log_error(source, dest, L" - " + translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_error_for") + source);
    }
}

void SyncBase::sync_file(const wstring& source, const wstring& dest, const wstring& wildcard)
{
    if (only_count_ && !planning()) {  // [SyncPlan.md#planning]
        ++copy_count_;
        return;
    }
    guarded(source, dest, [&] {
        SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, wildcard, Exclusions(),
                          sync_item_.flags(), sync_item_.compare_items()),
                 copy_count_, only_count_, false, state_, state_root_, target_root_)();
    });
}

void SyncBase::sync_link(const wstring& source, const wstring& dest)  // [SyncBase.md#sync_link]
{
    if (only_count_ && !planning()) {  // [SyncPlan.md#planning]
        ++copy_count_;
        return;
    }
    guarded(source, dest, [&] {
        SyncLink(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, L"", Exclusions(),
                          sync_item_.flags(), sync_item_.compare_items()),
                 copy_count_, only_count_, link_tally_, state_, state_root_, target_root_)();
    });
}

void SyncBase::sync_dir_entry(const fs::directory_entry& dir_entry, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_dir_entry: " + dir_entry.path().wstring());
    std::error_code ec;
    const bool symlink = dir_entry.is_symlink(ec);
    const bool directory = dir_entry.is_directory(ec);
    const bool as_link = symlink && links_as_links();  // [SyncBase.md#links_as_links]
    if ((directory && !as_link) || !dir_entry.path().has_filename())
        return;
    const wstring filename = dir_entry.path().filename().wstring();
    if ((!wildcard.empty() && !directory && !wildmatch(filename, wildcard)) || is_excluded(filename, sync_item_.exclusions())) {
        log_debug(L"sync_dir_entry: uebersprungen " + filename);
        return;
    }
    if (symlink && system_link_skipped(dir_entry.path().wstring()))
        return;
    const wstring target = with_trailing_char(destination, sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR) + filename;
    if (as_link)
        sync_link(dir_entry.path().wstring(), target);
    else if (target_usable(target))
        sync_file(dir_entry.path().wstring(), target, wildcard);
}

void SyncBase::sync_target_only_entries(const wstring& source_dir, const wstring& target_dir,
                                        const wstring& wildcard, const std::set<wstring>& source_names)
{
    if (!sync_item_.delete_in_target() && !sync_item_.two_way_sync())
        return;
    const wchar_t source_separator = sync_item_.ssh_source() ? L'/' : DIR_SEPARATOR;
    auto handle_entry = [&](const wstring& name, const wstring& target_path) {
        if (source_names.count(name) || (!wildcard.empty() && !wildmatch(name, wildcard))
            || is_excluded(name, sync_item_.exclusions())
            || (sync_item_.father_son_principle() && father_son_backup(name)))  // [SyncBase.md#father_son_backup]
            return;
        sync_file(with_trailing_char(source_dir, source_separator) + name, target_path, wildcard);
    };
    auto handle_link = [&](const wstring& name, const wstring& target_path, bool directory) {  // [SyncBase.md#target-only-links]
        if (source_names.count(name) || father_son_backup(name) || link_filtered(name, target_path, sync_item_.ssh_target(), directory, wildcard))
            return;
        sync_link(with_trailing_char(source_dir, source_separator) + name, target_path);
    };
    if (sync_item_.ssh_target()) {
        for (const auto& entry : ssh_list_dir(target_dir)) {
            if (entry.is_symlink && links_as_links())
                handle_link(entry.name, with_trailing_char(target_dir, L'/') + entry.name, false);
            else if (entry.is_regular_file)
                handle_entry(entry.name, with_trailing_char(target_dir, L'/') + entry.name);
            if (stop_requested())
                break;
        }
        return;
    }
    const wstring local_target_dir = without_volume_spec(target_dir);
    std::error_code ec;
    if (!fs::exists(local_target_dir, ec))
        return;
    for (const fs::directory_entry& dir_entry : fs::directory_iterator(local_target_dir)) {
        if (links_as_links() && dir_entry.is_symlink(ec))
            handle_link(dir_entry.path().filename().wstring(), dir_entry.path().wstring(), dir_entry.is_directory(ec));
        else if (dir_entry.is_regular_file(ec))
            handle_entry(dir_entry.path().filename().wstring(), dir_entry.path().wstring());
        if (stop_requested())
            break;
    }
}

bool father_son_backup(const wstring& name)  // [SyncBase.md#father_son_backup]
{
    const std::size_t suffix = name.rfind(FATHER_SON_SUFFIX);
    if (suffix == wstring::npos || suffix == 0)
        return false;
    const wstring generation = name.substr(suffix + std::char_traits<wchar_t>::length(FATHER_SON_SUFFIX));
    return !generation.empty() && std::all_of(generation.begin(), generation.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

bool SyncBase::stop_requested() const
{
    return Synchronize::stopping();
}

} // namespace AFS_Core
