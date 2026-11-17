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
#include "pch.h"
#include <SymlinkUtil.h>
#include <algorithm>
#include <array>
#include <string_view>
#ifdef WIN32
#include <windows.h>
#endif

namespace Utilities {

namespace symlink_util {

namespace fs = std::filesystem;
using FilesystemUtil::FileInfoResult;

namespace {
#if defined(__linux__)
constexpr std::array<std::string_view, 6> SYSTEM_ROOTS = {"/proc", "/sys", "/dev", "/run", "/var/run", "/snap"};  // [SymlinkUtil.md#system_link]
constexpr std::array<std::string_view, 3> USER_ROOTS = {"/run/media", "/run/mount", "/run/user"};  // [SymlinkUtil.md#system_link]
#elif defined(__APPLE__)
constexpr std::array<std::string_view, 3> SYSTEM_ROOTS = {"/System", "/dev", "/private/var/db"};
constexpr std::array<std::string_view, 0> USER_ROOTS = {};
#else
constexpr std::array<std::string_view, 0> SYSTEM_ROOTS = {};
constexpr std::array<std::string_view, 0> USER_ROOTS = {};
#endif

fs::path plain(const fs::path& path)  // [SymlinkUtil.md#lexical]
{
    const fs::path normal = path.lexically_normal();
    return normal.has_filename() || normal == normal.root_path() ? normal : normal.parent_path();
}

bool within(const fs::path& path, const fs::path& directory)  // [SymlinkUtil.md#lexical]
{
    return std::mismatch(directory.begin(), directory.end(), path.begin(), path.end()).first == directory.end();
}

bool below_any(const fs::path& path, const auto& roots)
{
    return std::ranges::any_of(roots, [&](std::string_view root) { return within(path, fs::path(root)); });
}

bool special_file(const fs::path& path)
{
    std::error_code ec;
    const fs::file_status status = fs::status(path, ec);
    return fs::is_block_file(status) || fs::is_character_file(status) || fs::is_fifo(status) || fs::is_socket(status);
}

fs::path physical(const fs::path& path)
{
    std::error_code ec;
    const fs::path resolved = fs::weakly_canonical(path, ec);
    return plain(ec ? path : resolved);
}

bool links_unknown_to_filesystem(const std::error_code& ec)  // [SymlinkUtil.md#create_link_refusal]
{
#ifdef WIN32
    if (ec.category() == std::system_category() && ec.value() == ERROR_PRIVILEGE_NOT_HELD)
        return true;
#endif
    return ec == std::errc::operation_not_permitted || ec == std::errc::not_supported || ec == std::errc::operation_not_supported
        || ec == std::errc::function_not_supported;
}

FileInfoResult reason_for_link_failure(const std::error_code& ec)
{
    if (!ec)
        return FileInfoResult::OK;
    return links_unknown_to_filesystem(ec) ? FileInfoResult::LinksNotSupported : FilesystemUtil::reason_for_write_failure(ec);
}

#ifdef _TEST
std::optional<std::error_code> link_error_for_test;
#endif
}

bool dangling_link(const fs::path& path)  // [SymlinkUtil.md#dangling_link]
{
    std::error_code ec;
    return fs::is_symlink(fs::symlink_status(path, ec)) && !fs::exists(path, ec);
}

LinkInfo link_info(const fs::path& path)  // [SymlinkUtil.md#link_info]
{
    std::error_code ec;
    if (!fs::is_symlink(fs::symlink_status(path, ec)))
        return {};
    const fs::file_status target = fs::status(path, ec);
    return {true, fs::read_symlink(path, ec), fs::exists(target), fs::is_directory(target)};
}

fs::path portable_link_text(const fs::path& link, const fs::path& text, const fs::path& root)  // [SymlinkUtil.md#portable_link_text]
{
    if (text.is_relative() || !within(plain(text), plain(root)))
        return text;
    return plain(text).lexically_relative(plain(link.parent_path()));
}

bool leaves_root(const fs::path& link, const fs::path& text, const fs::path& root)  // [SymlinkUtil.md#leaves_root]
{
    return !within(plain(text.is_absolute() ? text : link.parent_path() / text), plain(root));
}

bool system_link(const fs::path& link)  // [SymlinkUtil.md#system_link]
{
    std::error_code ec;
    if (!fs::is_symlink(fs::symlink_status(link, ec)))
        return false;
    const fs::path resolved = fs::weakly_canonical(link, ec);
    if (ec)
        return false;
    return special_file(resolved) || (below_any(resolved, SYSTEM_ROOTS) && !below_any(resolved, USER_ROOTS));
}

#ifdef _TEST
void set_link_error_for_test(std::optional<std::error_code> error)  // [SymlinkUtil.md#create_link_refusal]
{
    link_error_for_test = error;
}
#endif

FileInfoResult create_link_refusal(const fs::path& text, const fs::path& at, bool to_directory)  // [SymlinkUtil.md#create_link_refusal]
{
#ifdef _TEST
    if (link_error_for_test)
        return reason_for_link_failure(*link_error_for_test);
#endif
    std::error_code ec;
    if (to_directory)
        fs::create_directory_symlink(text, at, ec);
    else
        fs::create_symlink(text, at, ec);
    return reason_for_link_failure(ec);
}

LinkTrail::LinkTrail(const fs::path& source_root, const std::optional<fs::path>& target_root)
    : target_root_(target_root ? std::optional<fs::path>(physical(*target_root)) : std::nullopt), chain_{{physical(source_root), false}}
{
}

FollowRefusal LinkTrail::refusal(const fs::path& link) const  // [SymlinkUtil.md#LinkTrail]
{
    std::error_code ec;
    const fs::path target = fs::canonical(link, ec);
    if (ec || !fs::is_directory(target, ec))
        return FollowRefusal::Unresolved;
    if (std::ranges::any_of(chain_, [&](const Step& step) { return within(step.directory, target); }))
        return FollowRefusal::Loop;
    if (target_root_ && (within(target, *target_root_) || within(*target_root_, target)))
        return FollowRefusal::IntoTarget;
    const auto links = std::ranges::count_if(chain_, [](const Step& step) { return step.through_link; });
    return links >= MAX_FOLLOWED_LINKS ? FollowRefusal::TooManyLinks : FollowRefusal::None;
}

void LinkTrail::enter_link(const fs::path& link)
{
    chain_.push_back({physical(link), true});
}

void LinkTrail::enter_directory(const fs::path& name)
{
    chain_.push_back({chain_.back().directory / name, false});
}

void LinkTrail::leave()
{
    chain_.pop_back();
}

}

} // namespace Utilities
