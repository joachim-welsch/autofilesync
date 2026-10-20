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
#pragma once
#include "SyncBase.h"
#include <set>
#include <vector>

namespace AFS_Core {

class SyncTree: SyncBase {
public:
    enum class DirState { Both, NewInSource, NewInTarget, DeletedInSource, DeletedInTarget };  // [SyncTree.md#DirState]
private:
    struct Entry {  // common view of a local or SSH directory entry
        std::wstring name;
        bool is_directory;
        bool is_regular_file;
        bool is_symlink;
    };
    using Names = std::set<std::wstring>;
    wchar_t source_separator() const;
    wchar_t target_separator() const;
    std::vector<Entry> list_source(const std::wstring& sourcename) const;
    Names list_target_dirs(const std::wstring& targetname) const;  // [SyncTree.md#target-only-directories]
    bool known_below(const std::wstring& sourcename) const;
    DirState subtree_state(const std::wstring& name, const std::wstring& sourcename, const Names& target_dirs) const;
    void prepare(const std::wstring& sourcename, const std::wstring& targetname, DirState state);  // [SyncTree.md#prepare-and-settle]
    void settle(const std::wstring& sourcename, const std::wstring& targetname, DirState state);
    void sync_tree(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard, DirState state);
    Names sync_entries(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard, const Names& target_dirs);
    void sync_entry(const Entry& entry, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard, const Names& target_dirs);
    void sync_subtree(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard, const Names& target_dirs);
    void sync_file_entry(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_target_only_dirs(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard,
        const Names& source_names, const Names& target_dirs);  // [SyncTree.md#target-only-directories]
public:
    SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count, SyncState* state = nullptr);
    void operator()() override;
};

} // namespace AFS_Core
