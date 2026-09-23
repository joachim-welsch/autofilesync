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
#include "Synchronize.h"
#include "Translation.h"
#include "Util.h"
#include <filesystem>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncBase::SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count)
    : sync_item_(sync_item), copy_count_(copy_count), only_count_(only_count)
{}

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
        if (!ssh_exists(destination) && !ssh_mkdir(destination)) {
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

void SyncBase::sync_file(const wstring& source, const wstring& dest, const wstring& wildcard)
{
    if (only_count_) {
        ++copy_count_;
        return;
    }
    try {
        SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, wildcard, Exclusions(),
                          sync_item_.flags(), sync_item_.compare_items()),
                 copy_count_)();
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
            || is_excluded(name, sync_item_.exclusions()))
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

bool SyncBase::stop_requested() const
{
    return Synchronize::stopping();
}

} // namespace AFS_Core
