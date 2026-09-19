/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "Util.h"
#include "Logging.h"
#include "Translation.h"
#include "Global.h"
#include "CopyLog.h"

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncDir::SyncDir(const SyncItem& si, std::size_t& copy_count, bool only_count)
    : SyncBase(si, copy_count, only_count)
{}

void SyncDir::sync_ssh_dir_entry(const SshDirEntry& entry, const wstring& source_path, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_ssh_dir_entry: " + entry.name);
    if (!entry.is_regular_file || (!wildcard.empty() && !wildmatch(entry.name, wildcard)) || is_excluded(entry.name, sync_item_.exclusions()))
        return;
    const wstring fname = util::with_trailing_char(source_path, L'/') + entry.name;  // [SyncDir.md#ssh-source-local-target]
    const wstring dest = util::with_trailing_char(destination, sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR) + entry.name;
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
        create_target_dir_if_not_exist();
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
