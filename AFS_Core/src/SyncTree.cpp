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
    wstring root_path(const wstring& path, bool ssh)
    {
        return ssh ? path : without_volume_spec(path);
    }

    bool source_present(SyncTree::DirState state)
    {
        return state != SyncTree::DirState::NewInTarget && state != SyncTree::DirState::DeletedInSource;
    }

    bool target_present(SyncTree::DirState state)
    {
        return state != SyncTree::DirState::NewInSource && state != SyncTree::DirState::DeletedInTarget;
    }
}

SyncTree::SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count, SyncState* state)
    : SyncBase(si, copy_count, only_count, state)
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

SyncTree::Names SyncTree::list_target_dirs(const wstring& targetname) const  // [SyncTree.md#target-only-directories]
{
    Names dirs;
    if (sync_item_.ssh_target()) {
        for (const SshDirEntry& entry : ssh_list_dir(targetname))
            if (entry.is_directory && !entry.is_symlink)
                dirs.insert(entry.name);
        return dirs;
    }
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(targetname, ec))
        if (entry.is_directory(ec) && !entry.is_symlink(ec))
            dirs.insert(entry.path().filename().wstring());
    return dirs;
}

bool SyncTree::known_below(const wstring& sourcename) const
{
    return state_ != nullptr && state_->knows_below(state_key(sourcename));
}

SyncTree::DirState SyncTree::subtree_state(const wstring& name, const wstring& sourcename, const Names& target_dirs) const  // [SyncTree.md#DirState]
{
    if (!sync_item_.two_way_sync() || target_dirs.contains(name))  // a one-way item never lists the target, so it keeps today's path
        return DirState::Both;
    return known_below(with_trailing_char(sourcename, source_separator()) + name) ? DirState::DeletedInTarget : DirState::NewInSource;
}

void SyncTree::prepare(const wstring& sourcename, const wstring& targetname, DirState state)  // [SyncTree.md#prepare-and-settle]
{
    if (state == DirState::Both || state == DirState::NewInSource)
        create_target_dir_if_not_exist(targetname);
    else if (state == DirState::NewInTarget)
        create_source_dir(sourcename);
}

void SyncTree::settle(const wstring& sourcename, const wstring& targetname, DirState state)  // [SyncTree.md#prepare-and-settle]
{
    if (state != DirState::DeletedInSource && state != DirState::DeletedInTarget)
        return;
    const PendingDirRemoval removal{state == DirState::DeletedInSource ? targetname : sourcename,
        state == DirState::DeletedInSource ? sync_item_.ssh_target() : sync_item_.ssh_source()};
    if (state_ != nullptr)
        state_->defer(removal);  // after the deletions the pass put off, see SyncBase.md#finish_pass
    else
        remove_dir_if_empty(removal.path, removal.ssh);
}

void SyncTree::sync_tree(const wstring& sourcename, const wstring& targetname, const wstring& wildcard, DirState state)
{
    log_debug(L"sync_tree: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;
    try {
        prepare(sourcename, targetname, state);
        const Names target_dirs = sync_item_.two_way_sync() && target_present(state) ? list_target_dirs(targetname) : Names();
        const Names source_names = source_present(state) ? sync_entries(sourcename, targetname, wildcard, target_dirs) : Names();
        if (Synchronize::stopping())
            return;
        if (target_present(state))  // [SyncTree.md#target-only-files-at-this-level]
            sync_target_only_entries(sourcename, targetname, wildcard, source_names);
        sync_target_only_dirs(sourcename, targetname, wildcard, source_names, target_dirs);
        if (!Synchronize::stopping())
            settle(sourcename, targetname, state);
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

SyncTree::Names SyncTree::sync_entries(const wstring& sourcename, const wstring& targetname, const wstring& wildcard, const Names& target_dirs)  // [SyncTree.md#sync_entries]
{
    Names source_names;
    for (const Entry& entry : list_source(sourcename)) {
        source_names.insert(entry.name);
        sync_entry(entry, sourcename, targetname, wildcard, target_dirs);
        if (Synchronize::stopping())
            break;
    }
    return source_names;
}

void SyncTree::sync_entry(const Entry& entry, const wstring& sourcename, const wstring& targetname, const wstring& wildcard, const Names& target_dirs)
{
    if (entry.is_symlink && !entry.is_regular_file)  // [SyncTree.md#symlinks-never-followed]
        log_info(translation::translate("symlink_not_followed"), with_trailing_char(sourcename, source_separator()) + entry.name);
    else if (entry.is_directory)
        sync_subtree(entry.name, sourcename, targetname, wildcard, target_dirs);
    else if (entry.is_regular_file)
        sync_file_entry(entry.name, sourcename, targetname, wildcard);
}

void SyncTree::sync_subtree(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard, const Names& target_dirs)
{
    if (is_excluded(name, sync_item_.exclusions()))  // [SyncTree.md#excluded-directories]
        log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + name);
    else
        sync_tree(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard,
            subtree_state(name, sourcename, target_dirs));
}

void SyncTree::sync_file_entry(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    if ((!wildcard.empty() && !wildmatch(name, wildcard)) || is_excluded(name, sync_item_.exclusions()))
        return;
    sync_file(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard);
}

void SyncTree::sync_target_only_dirs(const wstring& sourcename, const wstring& targetname, const wstring& wildcard,
    const Names& source_names, const Names& target_dirs)  // [SyncTree.md#target-only-directories]
{
    for (const wstring& name : target_dirs) {
        if (source_names.contains(name) || is_excluded(name, sync_item_.exclusions()))
            continue;
        const wstring source = with_trailing_char(sourcename, source_separator()) + name;
        sync_tree(source, with_trailing_char(targetname, target_separator()) + name, wildcard,
            known_below(source) ? DirState::DeletedInSource : DirState::NewInTarget);
        if (Synchronize::stopping())
            break;
    }
}

void SyncTree::operator()()
{
    log_debug(L"SyncTree::operator()()");
    sync_tree(root_path(sync_item_.source(), sync_item_.ssh_source()), root_path(sync_item_.target(), sync_item_.ssh_target()), sync_item_.wildcard(), DirState::Both);
    finish_pass();
}

} // namespace AFS_Core
