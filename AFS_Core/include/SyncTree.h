/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once
#include "SyncBase.h"
#include <set>
#include <vector>

namespace AFS_Core {

class SyncTree: SyncBase {
    struct Entry {  // common view of a local or SSH directory entry
        std::wstring name;
        bool is_directory;
        bool is_regular_file;
        bool is_symlink;
    };
    wchar_t source_separator() const;
    wchar_t target_separator() const;
    std::vector<Entry> list_source(const std::wstring& sourcename) const;
    void sync_tree(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    std::set<std::wstring> sync_entries(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_entry(const Entry& entry, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_subtree(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_file_entry(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
public:
    SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count);
    void operator()() override;
};

} // namespace AFS_Core
