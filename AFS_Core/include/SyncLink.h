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
    struct Side {  // [SyncLink.md#decide]
        bool link = false;
        bool present = false;
        bool to_directory = false;
        std::filesystem::path text;
    };
    enum class Action { Leave, Settle, ToTarget, ToSource, DeleteInTarget, DeleteInSource, Conflict };  // [SyncLink.md#decide]
    LinkTally& tally_;
    std::wstring key_;

    Side side(const std::wstring& path, const std::wstring& root) const;
    std::optional<std::filesystem::path> base() const;  // [SyncLink.md#decide]
    Action decide(const Side& here, const Side& there) const;  // [SyncLink.md#decide]
    Action decide_two_links(const Side& here, const Side& there) const;
    Action decide_target_only(const Side& there) const;
    void carry_out(Action action, const std::wstring& source, const std::wstring& destination, const Side& here, const Side& there);
    void settle(const std::filesystem::path& text);
    void carry(const Side& from, const std::wstring& from_path, const std::wstring& to_path, bool to_target);  // [SyncLink.md#create]
    void remove(const std::wstring& source, const std::wstring& destination, bool in_target);  // [SyncLink.md#remove]
    void resolve_conflict(const std::wstring& source, const std::wstring& destination, const Side& here);  // [SyncLink.md#conflict]
    bool blocked_by_directory(const std::wstring& at) const;  // [SyncLink.md#blocked-by-directory]
    bool create(const std::filesystem::path& text, const std::wstring& at, bool to_directory);  // [SyncLink.md#create]
    fsu::FileInfoResult create_verified(const std::filesystem::path& text, const std::wstring& at, bool to_directory) const;  // [SyncLink.md#create]
    std::optional<std::wstring> park(const std::wstring& at) const;  // [SyncLink.md#park]
    bool put_in_place(const std::wstring& prepared, const std::wstring& at) const;
    void refused(const std::wstring& at, fsu::FileInfoResult refusal);
public:
    SyncLink(const SyncItem& si, std::size_t& copy_count, bool only_count, LinkTally& tally, SyncState* state,
        const std::wstring& state_root, const std::wstring& target_root);
    void operator()() override;
};

} // namespace AFS_Core
