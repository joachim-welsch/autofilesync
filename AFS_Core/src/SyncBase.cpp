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
#include "CopyLog.h"
#include "Global.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "SyncFile.h"
#include "SyncFileSsh.h"
#include "SyncPlan.h"
#include "Synchronize.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <filesystem>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

namespace {
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
    : sync_item_(sync_item), copy_count_(copy_count), only_count_(only_count), state_(state)
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

void SyncBase::finish_pass()  // [SyncBase.md#finish_pass]
{
    if (state_ == nullptr)
        return;
    const std::vector<PendingDeletion> deletions = state_->take_deletions();
    const std::vector<PendingDirRemoval> removals = state_->take_dir_removals();
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

void SyncBase::create_target_dir_if_not_exist(const std::wstring& targetdir)
{
    log_debug(L"create_target_dir_if_not_exist: " + targetdir);
    if (only_count_)
        return;
    const wstring destination = targetdir.empty() ? sync_item_.target() : targetdir;
    if (sync_item_.ssh_target()) {
        if (!ssh_mkdir_p(destination)) {
            log_error(translation::translate("ssh_dir_create_error") + destination);
            report_sync_error(translation::translate("ssh_target_dir_not_creatable") + destination);
        }
        return;
    }
    std::error_code ec;
    fs::create_directories(without_volume_spec(destination), ec);
    if (ec) {
        log_error(translation::translate("target_dir_create_error"), destination, string_to_wstring(ec.message()));
        report_sync_error(translation::translate("target_dir_not_creatable") + destination);
    }
}

void SyncBase::create_source_dir(const wstring& dir)  // [SyncBase.md#create_source_dir]
{
    log_debug(L"create_source_dir: " + dir);
    if (only_count_)
        return;
    std::error_code ec;
    const bool created = sync_item_.ssh_source() ? ssh_mkdir_p(dir) : (fs::create_directories(dir, ec), !ec);
    if (created)
        return;
    log_error(translation::translate("source_dir_create_error"), dir, string_to_wstring(ec.message()));
    report_sync_error(translation::translate("source_dir_create_error") + dir);
}

void SyncBase::remove_dir_if_empty(const wstring& dir, bool ssh)  // [SyncBase.md#remove_dir_if_empty]
{
    if (only_count_)
        return;
    std::error_code ec;
    if (ssh ? !ssh_list_dir(dir).empty() : !fs::is_empty(dir, ec) || ec)
        return;
    if (ssh ? ssh_rmdir(dir) : fs::remove(dir, ec) && !ec) {
        log_info(translation::translate("empty_dir_removed"), dir);
        return;
    }
    log_error(translation::translate("dir_delete_failed"), dir, string_to_wstring(ec.message()));
    report_sync_error(translation::translate("dir_delete_failed") + dir);
}

void SyncBase::sync_file(const wstring& source, const wstring& dest, const wstring& wildcard)
{
    if (only_count_ && !planning()) {  // [SyncPlan.md#planning]
        ++copy_count_;
        return;
    }
    try {
        SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, wildcard, Exclusions(),
                          sync_item_.flags(), sync_item_.compare_items()),
                 copy_count_, only_count_, false, state_, state_root_, target_root_)();
    } catch (const std::exception& e) {
        log_error(source, dest, string_to_wstring(e.what()));
        report_sync_error(source + L": " + string_to_wstring(e.what()));
    } catch (...) {
        log_error(source, dest, L" - " + translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_error_for") + source);
    }
}

void SyncBase::sync_dir_entry(const fs::directory_entry& dir_entry, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_dir_entry: " + dir_entry.path().wstring());
    std::error_code ec;
    if (dir_entry.is_directory(ec) || !dir_entry.path().has_filename())
        return;
    const wstring filename = dir_entry.path().filename().wstring();
    if ((!wildcard.empty() && !wildmatch(filename, wildcard)) || is_excluded(filename, sync_item_.exclusions())) {
        log_debug(L"sync_dir_entry: uebersprungen " + filename);
        return;
    }
    const wchar_t separator = sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR;
    sync_file(dir_entry.path().wstring(), with_trailing_char(destination, separator) + filename, wildcard);
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
    if (sync_item_.ssh_target()) {
        for (const auto& entry : ssh_list_dir(target_dir)) {
            if (entry.is_regular_file)
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
        if (dir_entry.is_regular_file(ec))
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
