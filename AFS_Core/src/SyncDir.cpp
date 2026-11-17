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
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "StrUtil.h"
#include <filesystem>
#include "Logging.h"
#include "Translation.h"
#include "Global.h"
#include "CopyLog.h"

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncDir::SyncDir(const SyncItem& si, std::size_t& copy_count, bool only_count, SyncState* state)
    : SyncBase(si, copy_count, only_count, state)
{}

bool SyncDir::followed_file_link(const SshDirEntry& entry, const wstring& source_path) const  // [SyncDir.md#followed-file-links]
{
    if (!entry.is_symlink || !follows_links())
        return false;
    const std::optional<SshLink> link = ssh_link_info(util::with_trailing_char(source_path, L'/') + entry.name);
    return link && link->resolves && !link->to_directory;
}

bool SyncDir::filtered(const SshDirEntry& entry, const wstring& source_path, const wstring& wildcard) const
{
    if (!entry.is_regular_file && !followed_file_link(entry, source_path))
        return true;
    return (!wildcard.empty() && !wildmatch(entry.name, wildcard)) || is_excluded(entry.name, sync_item_.exclusions());
}

void SyncDir::sync_ssh_dir_entry(const SshDirEntry& entry, const wstring& source_path, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_ssh_dir_entry: " + entry.name);
    const bool as_link = entry.is_symlink && links_as_links();  // [SyncBase.md#links_as_links]
    const wstring fname = util::with_trailing_char(source_path, L'/') + entry.name;  // [SyncDir.md#ssh-source-local-target]
    if (as_link ? link_filtered(entry.name, fname, true, false, wildcard) : filtered(entry, source_path, wildcard))
        return;
    const wstring dest = util::with_trailing_char(destination, sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR) + entry.name;
    if (as_link)
        sync_link(fname, dest);
    else
        sync_file(fname, dest, wildcard);
}

void SyncDir::operator()()
{
    log_debug(L"SyncDir::operator()()");
    const wstring source = sync_item_.source();
    if (source.empty())
        return;
    const wstring destination = sync_item_.target();
    const wstring wildcard = sync_item_.wildcard();
    std::set<wstring> source_names;  // [SyncDir.md#source-names]
    try {
        if (!create_target_dir_if_not_exist())
            return;
        if (sync_item_.ssh_source()) {
            for (const auto& entry : ssh_list_dir(source)) {
                source_names.insert(entry.name);
                sync_ssh_dir_entry(entry, source, wildcard, destination);
                if (stop_requested())
                    break;
            }
        } else {
            const wstring local_dest = sync_item_.ssh_target() ? destination : without_volume_spec(destination);
            for (const fs::directory_entry& dir_entry : fs::directory_iterator(without_volume_spec(source))) {
                source_names.insert(dir_entry.path().filename().wstring());
                sync_dir_entry(dir_entry, wildcard, local_dest);
                if (stop_requested())
                    break;
            }
        }
        if (!stop_requested())
            sync_target_only_entries(source, destination, wildcard, source_names);
        finish_pass();
    }
    catch (const fs::filesystem_error& e) {
        report_failure(source, e);
    }
    catch (const std::exception& e) {
        log_error(source, destination, string_to_wstring(e.what()));
        report_sync_error(source + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        log_error(source, destination, L" - " + translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_error_for") + source);
    }
}

} // namespace AFS_Core
