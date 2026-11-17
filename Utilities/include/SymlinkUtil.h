/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <UtilitiesConfig.h>
#include <FilesystemUtil.h>
#include <filesystem>
#include <optional>
#include <system_error>
#include <vector>

namespace Utilities {

namespace symlink_util {

    struct LinkInfo {  // [SymlinkUtil.md#link_info]
        bool is_link = false;
        std::filesystem::path text;
        bool resolves = false;
        bool to_directory = false;
    };

    inline constexpr int MAX_FOLLOWED_LINKS = 16;  // [SymlinkUtil.md#LinkTrail]
    enum class FollowRefusal { None, Unresolved, Loop, IntoTarget, TooManyLinks };  // [SymlinkUtil.md#LinkTrail]

    class LinkTrail {  // [SymlinkUtil.md#LinkTrail]
        struct Step {
            std::filesystem::path directory;
            bool through_link;
        };
        std::optional<std::filesystem::path> target_root_;
        std::vector<Step> chain_;
    public:
        LinkTrail(const std::filesystem::path& source_root, const std::optional<std::filesystem::path>& target_root);
        FollowRefusal refusal(const std::filesystem::path& link) const;
        void enter_link(const std::filesystem::path& link);
        void enter_directory(const std::filesystem::path& name);
        void leave();
    };

    bool dangling_link(const std::filesystem::path& path);  // [SymlinkUtil.md#dangling_link]
    LinkInfo link_info(const std::filesystem::path& path);  // [SymlinkUtil.md#link_info]
    std::filesystem::path portable_link_text(const std::filesystem::path& link, const std::filesystem::path& text, const std::filesystem::path& root);  // [SymlinkUtil.md#portable_link_text]
    bool leaves_root(const std::filesystem::path& link, const std::filesystem::path& text, const std::filesystem::path& root);  // [SymlinkUtil.md#leaves_root]
    bool system_link(const std::filesystem::path& link);  // [SymlinkUtil.md#system_link]
    FilesystemUtil::FileInfoResult create_link_refusal(const std::filesystem::path& text, const std::filesystem::path& at, bool to_directory);  // [SymlinkUtil.md#create_link_refusal]
#ifdef _TEST
    void set_link_error_for_test(std::optional<std::error_code> error);  // [SymlinkUtil.md#create_link_refusal]
#endif
}

} // namespace Utilities
