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
#include "pch.h"
#include "Restore.h"
#include "CheckPath.h"
#include "Logging.h"
#include "RecycleBin.h"
#include "StrUtil.h"
#include "SyncBase.h"
#include "Translation.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <system_error>

namespace AFS_Core {

namespace restore {

namespace fs = std::filesystem;
using std::chrono::system_clock;

namespace {
struct Stamped {
    system_clock::time_point time;
    std::wstring name;
};

struct Place {  // [Restore.md#Place]
    SyncItemType type = SyncItemType::Undefined;
    fs::path target;
    fs::path source;
};

std::optional<Stamped> stamped(const std::wstring& name)  // [Restore.md#stamped]
{
    static const std::wregex pattern(LR"((\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})-(\d{2})-(\d{3})_(.+))");
    std::wsmatch parts;
    if (!std::regex_match(name, parts, pattern))
        return std::nullopt;
    const auto number = [&](int group) { return std::stoi(parts[group].str()); };
    std::tm local{};
    local.tm_year = number(1) - 1900;
    local.tm_mon = number(2) - 1;
    local.tm_mday = number(3);
    local.tm_hour = number(4);
    local.tm_min = number(5);
    local.tm_sec = number(6);
    local.tm_isdst = -1;
    const std::time_t seconds = std::mktime(&local);
    if (seconds == static_cast<std::time_t>(-1))
        return std::nullopt;
    return Stamped{system_clock::from_time_t(seconds) + std::chrono::milliseconds(number(7)), parts[8].str()};
}

std::optional<Stamped> stamped_origin(const fs::path& bin_item, fs::path& origin)  // [Restore.md#stamped_origin]
{
    if (const auto file = stamped(bin_item.filename().wstring())) {
        origin = bin_item.parent_path() / file->name;
        return file;
    }
    std::optional<Stamped> found;
    for (const fs::path& part : bin_item) {
        const auto directory = found ? std::nullopt : stamped(part.wstring());
        origin /= directory ? fs::path(directory->name) : part;
        if (directory)
            found = directory;
    }
    return found;
}

std::optional<Place> place_of(const SyncItem& item)
{
    if (item.ssh_source() || item.ssh_target() || item.source().empty() || item.target().empty())
        return std::nullopt;
    return Place{item.sync_item_type(), fs::path(without_volume_spec(item.target())).lexically_normal(), fs::path(without_volume_spec(item.source())).lexically_normal()};
}

std::optional<fs::path> below(const fs::path& directory, const fs::path& path)
{
    const fs::path relative = path.lexically_relative(directory);
    if (relative.empty() || relative == "." || *relative.begin() == "..")
        return std::nullopt;
    return relative;
}

std::optional<fs::path> in_source(const Place& place, const fs::path& target_file)  // [Restore.md#Place]
{
    const auto relative = below(place.target.relative_path(), target_file.relative_path());
    if (!relative)
        return std::nullopt;
    const bool direct_child = !relative->has_parent_path();
    switch (place.type) {
    case SyncItemType::Tree:
        return place.source / *relative;
    case SyncItemType::Dir:
        return direct_child ? std::optional(place.source / *relative) : std::nullopt;
    case SyncItemType::File:
        return direct_child && *relative == place.source.filename() ? std::optional(place.source) : std::nullopt;
    default:
        return std::nullopt;
    }
}

fs::path home_of(const std::vector<Place>& places, const fs::path& origin)  // [Restore.md#home_of]
{
    const Place* best = nullptr;
    for (const Place& place : places)
        if (in_source(place, origin) && (best == nullptr || place.target.native().size() > best->target.native().size()))
            best = &place;
    if (best != nullptr)
        return *in_source(*best, origin);
#ifdef _WIN32
    return origin.relative_path();
#else
    return fs::path("/") / origin.relative_path();
#endif
}

std::uintmax_t size_of(const fs::path& file)
{
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    return ec ? 0 : size;
}

void add_deleted(Restorables& found, const std::vector<Place>& places)
{
    const fs::path bin = RecycleBin::instance().bin_path();
    for (const fs::path& item : RecycleBin::instance().list_items()) {
        fs::path origin;
        if (const auto stamp = stamped_origin(item, origin))
            found.push_back({RestorableKind::Deleted, bin / item, home_of(places, origin), stamp->time, size_of(bin / item)});
    }
}

fs::path replaced_file(const fs::path& version)
{
    const std::wstring name = version.filename().wstring();
    return version.parent_path() / name.substr(0, name.rfind(FATHER_SON_SUFFIX));
}

system_clock::time_point written(const fs::directory_entry& entry)
{
    std::error_code ec;
    const auto file_time = entry.last_write_time(ec);
    return ec ? system_clock::time_point() : std::chrono::time_point_cast<system_clock::duration>(std::chrono::file_clock::to_sys(file_time));
}

template <typename Iterator>
void add_versions(Restorables& found, const fs::path& target, const std::optional<Place>& place, const std::atomic<bool>& stop)  // [Restore.md#versions]
{
    std::error_code ec;
    for (Iterator it(target, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end && !stop; it.increment(ec)) {
        if (!it->is_regular_file(ec) || !father_son_backup(it->path().filename().wstring()))
            continue;
        const fs::path replaced = replaced_file(it->path());
        const auto source = place ? in_source(*place, replaced) : std::nullopt;
        found.push_back({RestorableKind::EarlierVersion, it->path(), source.value_or(replaced), written(*it), size_of(it->path())});
    }
}

void add_versions(Restorables& found, const SyncItem& item, const std::atomic<bool>& stop)
{
    if (!item.father_son_principle() || item.ssh_target() || item.target().empty())
        return;
    const fs::path target = fs::path(without_volume_spec(item.target())).lexically_normal();
    if (item.sync_item_type() == SyncItemType::Tree)
        add_versions<fs::recursive_directory_iterator>(found, target, place_of(item), stop);
    else
        add_versions<fs::directory_iterator>(found, target, place_of(item), stop);
}

std::wstring minute_of(system_clock::time_point time)
{
    const std::time_t seconds = system_clock::to_time_t(time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    std::wostringstream text;
    text << std::put_time(&local, L"%Y-%m-%d %H-%M");
    return text.str();
}

bool taken(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));  // [Restore.md#free_name]
}

fs::path free_name(const fs::path& directory, const Restorable& file)  // [Restore.md#free_name]
{
    const fs::path name = file.home.filename();
    if (!taken(directory / name))
        return directory / name;
    const std::wstring dated = name.stem().wstring() + L" (" + minute_of(file.time) + L")";
    fs::path candidate = directory / (dated + name.extension().wstring());
    for (int number = 2; taken(candidate); ++number)
        candidate = directory / (dated + L" " + std::to_wstring(number) + name.extension().wstring());
    return candidate;
}

bool copy_version(const fs::path& version, const fs::path& destination)  // [Restore.md#copy_version]
{
    std::error_code ec;
    fs::copy_file(version, destination, fs::copy_options::none, ec);
    if (ec) {
        log_error(translation::translate("copying_label"), version.wstring(), destination.wstring(), util::string_to_wstring(ec.message()));
        return false;
    }
    fs::last_write_time(destination, fs::file_time_type::clock::now(), ec);
    return true;
}

bool take_from_bin(const fs::path& stored, const fs::path& destination)
{
    return RecycleBin::instance().take_item(stored.lexically_relative(RecycleBin::instance().bin_path()), destination);
}
}

Restorables find(const SyncItems& items, const std::atomic<bool>& stop)
{
    std::vector<Place> places;
    for (const SyncItem& item : items)
        if (const auto place = place_of(item))
            places.push_back(*place);
    Restorables found;
    add_deleted(found, places);
    for (const SyncItem& item : items)
        add_versions(found, item, stop);
    std::ranges::sort(found, [](const Restorable& a, const Restorable& b) { return a.time > b.time; });
    return found;
}

bool place_known(const Restorable& file)
{
    return file.home.is_absolute();
}

std::optional<fs::path> put_back(const Restorable& file, const fs::path& directory)
{
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        log_error(translation::translate("restore_dir_not_creatable"), directory.wstring(), util::string_to_wstring(ec.message()));
        return std::nullopt;
    }
    const fs::path destination = free_name(directory, file);
    const bool done = file.kind == RestorableKind::Deleted ? take_from_bin(file.stored, destination) : copy_version(file.stored, destination);
    if (!done)
        return std::nullopt;
    log_info(translation::translate("restore_done"), file.stored.wstring(), destination.wstring());
    return destination;
}

}

} // namespace AFS_Core
