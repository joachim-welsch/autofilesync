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
#include "SyncTree.h"
#include "Global.h"
#include "SyncFileSsh.h"
#include "Util.h"
#include "Synchronize.h"
#include "Logging.h"
#include "Translation.h"
#include "CopyLog.h"
#include <filesystem>

namespace AFS_Core {

using std::wstring;
using namespace util;
namespace fs = std::filesystem;

namespace {
    wstring side_path(const wstring& path, bool ssh)
    {
        return ssh ? path : without_volume_spec(path);
    }
}

SyncTree::SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count)
    : SyncBase(si, copy_count, only_count)
{}

wchar_t SyncTree::source_separator() const  // SSH/SFTP paths always use '/', regardless of the client platform
{
    return sync_item_.ssh_source() ? L'/' : DIR_SEPARATOR;
}

wchar_t SyncTree::target_separator() const
{
    return sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR;
}

std::vector<SyncTree::Entry> SyncTree::list_source(const wstring& sourcename) const
{
    std::vector<Entry> entries;
    if (sync_item_.ssh_source()) {
        for (const SshDirEntry& entry : ssh_list_dir(sourcename))
            entries.push_back({entry.name, entry.is_directory, entry.is_regular_file, entry.is_symlink});
        return entries;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(with_trailing_char(sourcename, DIR_SEPARATOR))) {
        try {
            entries.push_back({entry.path().filename().wstring(), entry.is_directory(), entry.is_regular_file(), entry.is_symlink()});
        } catch (const std::exception& e) {  // one unreadable entry must not hide the rest of the directory
            log_error(sourcename, string_to_wstring(e.what()));
        }
    }
    return entries;
}

void SyncTree::sync_tree(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_tree: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;
    try {
        create_target_dir_if_not_exist(targetname);
        const std::set<wstring> source_names = sync_entries(sourcename, targetname, wildcard);
        if (!Synchronize::stopping())  // [SyncTree.md#target-only-files-at-this-level]
            sync_target_only_entries(sourcename, targetname, wildcard, source_names);
    }
    catch (const std::exception& e) {
        log_error(sourcename, targetname, string_to_wstring(e.what()));
        report_sync_error(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        log_error(sourcename, targetname, L" - " + translation::translate("unknown_error"));
        report_sync_error(translation::translate("unknown_error_for") + sourcename);
    }
}

std::set<wstring> SyncTree::sync_entries(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)  // [SyncTree.md#sync_entries]
{
    std::set<wstring> source_names;
    for (const Entry& entry : list_source(sourcename)) {
        source_names.insert(entry.name);
        sync_entry(entry, sourcename, targetname, wildcard);
        if (Synchronize::stopping())
            break;
    }
    return source_names;
}

void SyncTree::sync_entry(const Entry& entry, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    if (entry.is_symlink && !entry.is_regular_file)  // [SyncTree.md#symlinks-never-followed]
        log_info(translation::translate("symlink_not_followed"), with_trailing_char(sourcename, source_separator()) + entry.name);
    else if (entry.is_directory)
        sync_subtree(entry.name, sourcename, targetname, wildcard);
    else if (entry.is_regular_file)
        sync_file_entry(entry.name, sourcename, targetname, wildcard);
}

void SyncTree::sync_subtree(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    if (is_excluded(name, sync_item_.exclusions()))  // [SyncTree.md#excluded-directories]
        log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + name);
    else
        sync_tree(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard);
}

void SyncTree::sync_file_entry(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    if ((!wildcard.empty() && !wildmatch(name, wildcard)) || is_excluded(name, sync_item_.exclusions()))
        return;
    sync_file(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard);
}

void SyncTree::operator()()
{
    log_debug(L"SyncTree::operator()()");
    sync_tree(side_path(sync_item_.source(), sync_item_.ssh_source()), side_path(sync_item_.target(), sync_item_.ssh_target()), sync_item_.wildcard());
}

} // namespace AFS_Core
