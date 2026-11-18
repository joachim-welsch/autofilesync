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
#include "RecycleBin.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "Translation.h"
#include "StrUtil.h"
#include <filesystem>
#include <system_error>

namespace Utilities {

namespace fs = std::filesystem;

namespace {
bool entry_exists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec)) && !ec;
}

fsu::FileInfoResult create_parent(const fs::path& path)
{
    if (path.parent_path().empty())
        return fsu::FileInfoResult::OK;
    std::error_code ec;
    const fsu::FileInfoResult refusal = fsu::create_directory_refusal(path.parent_path(), ec);
    if (refusal != fsu::FileInfoResult::OK)
        log_fatal(translation::translate("recycle_bin_dir_not_creatable"), path.parent_path().wstring());
    return refusal;
}
}

RecycleBin::RecycleBin()
{
    const std::string home = fsu::home_dir();
    path_ = home.empty() ? fs::path(".recyclebin") : fs::path(home) / ".recyclebin";
}

RecycleBin::~RecycleBin() = default;

RecycleBin& RecycleBin::instance()
{
    static RecycleBin obj;
    return obj;
}

bool RecycleBin::check_relative(const fs::path& item) const  // [RecycleBin.md#check_relative]
{
    if (item.empty() || item.is_absolute() || item.has_root_name()) {
        log_error(translation::translate("recycle_bin_absolute_path"), item.wstring());
        return false;
    }
    for (const auto& part : item.lexically_normal())
        if (part == "..") {
            log_error(translation::translate("recycle_bin_path_escapes"), item.wstring());
            return false;
        }
    return true;
}

static bool real_directory(const fs::path& path)
{
    std::error_code ec;
    return fs::is_directory(fs::symlink_status(path, ec));
}

static void discard_copy(const fs::path& copy)  // [RecycleBin.md#move-takes-the-copy-back]
{
    std::error_code ec;
    fs::remove_all(copy, ec);
}

static bool lacks_room(const fs::path& from, const fs::path& to)  // [RecycleBin.md#no-room]
{
    std::error_code ec;
    if (!fs::is_regular_file(fs::symlink_status(from, ec)))
        return false;
    const std::uintmax_t size = fs::file_size(from, ec);
    const std::optional<uint64_t> room = fsu::free_space(to.parent_path());
    return !ec && room && size > *room;
}

static void keep_write_times(const fs::path& from, const fs::path& to)  // [RecycleBin.md#move]
{
    std::error_code ec;
    if (fs::is_symlink(from, ec))
        return;
    fs::last_write_time(to, fs::last_write_time(from, ec), ec);
    if (!fs::is_directory(from, ec))
        return;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(from, ec))
        if (!entry.is_symlink(ec))
            fs::last_write_time(to / entry.path().lexically_relative(from), entry.last_write_time(ec), ec);
}

fsu::FileInfoResult RecycleBin::move(const fs::path& from, const fs::path& to) const  // [RecycleBin.md#move]
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec)
        return fsu::FileInfoResult::OK;
    if (lacks_room(from, to))  // [RecycleBin.md#no-room]
        return fsu::FileInfoResult::DiskFull;
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        log_error(translation::translate("recycle_bin_move_failed"), from.wstring(), to.wstring(), util::string_to_wstring(ec.message()));
        discard_copy(to);
        return fsu::reason_for_write_failure(ec);
    }
    keep_write_times(from, to);
    const bool whole_while_there = !real_directory(from);  // [RecycleBin.md#move-takes-the-copy-back]
    fs::remove_all(from, ec);
    if (!ec)
        return fsu::FileInfoResult::OK;
    log_error(translation::translate("recycle_bin_source_not_deletable"), from.wstring());
    if (whole_while_there && entry_exists(from))
        discard_copy(to);
    return fsu::reason_for_write_failure(ec);
}

void RecycleBin::bin_path(const fs::path& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
}

fs::path RecycleBin::bin_path() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

bool RecycleBin::add_item(const fs::path& item)
{
    return add_item(item, item);
}

bool RecycleBin::add_item(const fs::path& source, const fs::path& item)
{
    return add_refusal(source, item) == fsu::FileInfoResult::OK;
}

fsu::FileInfoResult RecycleBin::add_refusal(const fs::path& source, const fs::path& item)  // [RecycleBin.md#add_refusal]
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!check_relative(item))
        return fsu::FileInfoResult::ErrorWriteFileInfo;
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(source, ec);
    if (status.type() == fs::file_type::not_found)  // [RecycleBin.md#add_refusal]
        return fsu::FileInfoResult::FileNotExist;
    if (!fs::exists(status)) {
        log_error(translation::translate("recycle_bin_file_not_exist"), source.wstring());
        return fsu::reason_for_write_failure(ec);
    }
    if (fsu::read_only_filesystem(source.parent_path()))  // [RecycleBin.md#read-only-source]
        return fsu::FileInfoResult::ReadOnlyFilesystem;
    const fs::path target = path_ / item.lexically_normal();
    if (entry_exists(target)) {  // [RecycleBin.md#add_item-never-replaces]
        log_error(translation::translate("recycle_bin_target_exists"), target.wstring());
        return fsu::FileInfoResult::ErrorWriteFileInfo;
    }
    const fsu::FileInfoResult no_parent = create_parent(target);
    const fsu::FileInfoResult refusal = no_parent == fsu::FileInfoResult::OK ? move(source, target) : no_parent;
    if (refusal == fsu::FileInfoResult::OK)
        log_debug(L"Papierkorb: verschoben " + source.wstring());
    return refusal == fsu::FileInfoResult::DiskFull ? fsu::FileInfoResult::RecycleBinFull : refusal;  // [RecycleBin.md#no-room]
}

bool RecycleBin::has_item(const fs::path& item) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entry_exists(path_ / item.lexically_normal());
}

bool RecycleBin::restore_item(const fs::path& item)
{
    return take_item(item, item.lexically_normal());
}

bool RecycleBin::take_item(const fs::path& item, const fs::path& target)  // [RecycleBin.md#take_item]
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!check_relative(item))
        return false;
    const fs::path source = path_ / item.lexically_normal();
    if (!entry_exists(source)) {
        log_error(translation::translate("recycle_bin_file_not_inside"), item.wstring());
        return false;
    }
    if (entry_exists(target)) {
        log_error(translation::translate("recycle_bin_target_exists"), target.wstring());
        return false;
    }
    if (create_parent(target) != fsu::FileInfoResult::OK || move(source, target) != fsu::FileInfoResult::OK)
        return false;
    log_debug(L"Papierkorb: wiederhergestellt " + target.wstring());
    return true;
}

RecycleBin::ItemList RecycleBin::list_items() const  // [RecycleBin.md#list_items]
{
    std::lock_guard<std::mutex> lock(mutex_);
    ItemList items;
    std::error_code ec;
    if (!fs::exists(path_, ec))
        return items;
    for (auto it = fs::recursive_directory_iterator(path_, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            log_error(translation::translate("recycle_bin_dir_unreadable"), path_.wstring());
            break;
        }
        if (!it->is_directory(ec))
            items.push_back(it->path().lexically_relative(path_));
    }
    return items;
}

} // namespace Utilities
