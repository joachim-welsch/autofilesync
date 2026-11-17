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
#include <FilesystemUtil.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AFS_Core {

class SyncLink: public SyncBase {  // [SyncLink.md#SyncLink]
    LinkTally& tally_;

    bool same_link(const std::wstring& destination, const std::filesystem::path& text) const;
    bool blocked_by_directory(const std::wstring& destination) const;  // [SyncLink.md#blocked-by-directory]
    void plan(const std::wstring& source, const std::wstring& destination);
    bool create(const std::filesystem::path& text, const std::wstring& destination, bool to_directory);  // [SyncLink.md#create]
    fsu::FileInfoResult create_verified(const std::filesystem::path& text, const std::wstring& at, bool to_directory) const;  // [SyncLink.md#create]
    std::optional<std::wstring> park(const std::wstring& destination) const;  // [SyncLink.md#park]
    bool put_in_place(const std::wstring& prepared, const std::wstring& destination) const;
    void refused(const std::wstring& destination, fsu::FileInfoResult refusal);
    void created(const std::wstring& source, const std::wstring& destination, const std::filesystem::path& text);
public:
    SyncLink(const SyncItem& si, std::size_t& copy_count, bool only_count, LinkTally& tally, SyncState* state,
        const std::wstring& state_root, const std::wstring& target_root);
    void operator()() override;
};

} // namespace AFS_Core
