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
    struct Place {  // [SyncLink.md#places]
        std::wstring path;
        std::wstring root;
        bool ssh = false;
    };
    struct Side {  // [SyncLink.md#decide]
        bool link = false;
        bool present = false;
        bool directory = false;
        bool to_directory = false;
        std::filesystem::path text;
    };
    enum class Action { Leave, Settle, ToTarget, ToSource, DeleteInTarget, DeleteInSource, Conflict };  // [SyncLink.md#decide]
    LinkTally& tally_;
    std::wstring key_;
    Place source_;
    Place target_;

    std::optional<Side> side(const Place& place) const;  // [SyncLink.md#places]
    std::optional<Side> unasked(const Place& place, fsu::FileInfoResult refusal) const;  // [SyncLink.md#unasked]
    std::optional<std::filesystem::path> base() const;  // [SyncLink.md#decide]
    Action decide(const Side& here, const Side& there) const;  // [SyncLink.md#decide]
    Action decide_two_links(const Side& here, const Side& there) const;
    Action decide_target_only(const Side& there) const;
    void carry_out(Action action, const Side& here, const Side& there);
    void settle(const std::filesystem::path& text);
    void carry(const Side& from, const Place& origin, const Side& to, const Place& destination);  // [SyncLink.md#create]
    void plan(const Place& destination) const;
    void carried(const Side& from, const Place& origin, const Place& destination);
    void remove(bool in_target);  // [SyncLink.md#remove]
    void resolve_conflict(const Side& here, const Side& there);  // [SyncLink.md#conflict]
    bool blocked_by_directory(const Side& side, const Place& place) const;  // [SyncLink.md#blocked-by-directory]
    bool create(const std::filesystem::path& text, bool to_directory, const Place& at);  // [SyncLink.md#create]
    std::optional<std::wstring> park(const Place& at) const;  // [SyncLink.md#park]
    bool put_in_place(const std::wstring& prepared, const Place& at) const;
    void refused(const Place& at, fsu::FileInfoResult refusal);
public:
    SyncLink(const SyncItem& si, std::size_t& copy_count, bool only_count, LinkTally& tally, SyncState* state,
        const std::wstring& state_root, const std::wstring& target_root);
    void operator()() override;
};

} // namespace AFS_Core
