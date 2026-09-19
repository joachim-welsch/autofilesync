/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "CheckPath.h"
#include <sstream>
#include "Logging.h"
#include "Util.h"
#include "MountedDrives.h"
#include "Config.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#include <filesystem>
#include <optional>

namespace AFS_Core {

using namespace util;
using std::wstring;
namespace fs = std::filesystem;

namespace {
    constexpr uint16_t BASE_INTERVAL = 2;
    constexpr uint16_t MAX_INTERVAL_REACHABLE = 3600;  // re-checking rarely is cheap and does not change the outcome
    constexpr uint16_t MAX_INTERVAL_UNREACHABLE = 300;  // [CheckPath.md#max-interval-unreachable]

    std::optional<SshAccount> ssh_account_of_path(const wstring& path, wstring& remote_path)  // [CheckPath.md#ssh_account_of_path]
    {
        const auto colon_pos = path.find(L':');
        if (colon_pos == wstring::npos || colon_pos == 0)
            return std::nullopt;
        const wstring account_name = path.substr(0, colon_pos);
        auto account = Config::instance().find_ssh_account_by_name(wstring_to_string(account_name));
        if (!account.has_value())
            return std::nullopt;
        remote_path = path.substr(colon_pos + 1);
        return account;
    }
}

bool path_contains_volume_name(const wstring& path)
{
    const auto open_parenthesis_pos = path.find(L'(');
    const auto colon_pos = path.find(L':');
    const bool retval = open_parenthesis_pos != wstring::npos && colon_pos != wstring::npos && open_parenthesis_pos < colon_pos;
    log_debug("path_contains_volume_name " + wstring_to_string(path) + ": ", retval);
    return retval;
}

wstring volume_name(const wstring& path)
{
    log_debug(L"volume_name() " + path);
    if (!path_contains_volume_name(path))
        return {};
    return path.substr(0, path.find(L'(') - 1);
}

uint16_t next_check_interval(uint16_t previous_interval, bool repeats_previous_result, bool reachable)
{
    if (!repeats_previous_result)
        return BASE_INTERVAL;
    const uint16_t max_interval = reachable ? MAX_INTERVAL_REACHABLE : MAX_INTERVAL_UNREACHABLE;
    const uint32_t doubled = static_cast<uint32_t>(previous_interval) * 2u;
    return doubled > max_interval ? max_interval : static_cast<uint16_t>(doubled);
}

bool CheckPath::path_exists(const wstring& pathname, bool must_exist) const
{
    log_debug(L"drivesExists()");
#ifdef _UNIX
    (void)must_exist;
    std::error_code ec;
    return fs::exists(pathname, ec);
#else
    const wstring drive_name = root_name(pathname);
    if (path_contains_volume_name(pathname) && !volume_exists(drive_name, volume_name(pathname)))
        return false;
    if (!must_exist)  // [CheckPath.md#target-need-not-exist]
        return drive_exists(drive_name);
    std::error_code ec;
    return fs::exists(without_volume_spec(pathname), ec);
#endif
}

bool CheckPath::ssh_path_reachable(const wstring& pathname, bool must_exist) const
{
    log_debug(L"ssh_path_reachable: " + pathname);
    wstring remote_path;
    auto account = ssh_account_of_path(pathname, remote_path);
    if (!account.has_value()) {  // no account for this prefix: nothing can be resolved
        log_error(translation::translate("ssh_account_not_found_for") + pathname);
        return false;
    }
    try {
        SshFileInspector inspector(account.value());  // [CheckPath.md#ssh-inspector-connects]
        if (!must_exist)
            return true;
        const RemoteFileInfo info = inspector.inspect_file(wstring_to_string(remote_path));  // [CheckPath.md#remote-absence-is-definite]
        if (!info.exists)
            log_info(translation::translate("ssh_path_not_exist") + pathname);
        return info.exists;
    }
    catch (const std::exception& e) {
        log_info(translation::translate("ssh_path_not_reachable") + pathname + L" - " + string_to_wstring(e.what()));
        return false;
    }
    catch (...) {
        log_info(translation::translate("ssh_path_not_reachable") + pathname);
        return false;
    }
}

bool CheckPath::side_reachable(const wstring& pathname, bool must_exist) const
{
    wstring remote_path;
    if (ssh_account_of_path(pathname, remote_path).has_value())
        return ssh_path_reachable(pathname, must_exist);
    return path_exists(pathname, must_exist);
}

bool CheckPath::expired(const DriveCheckCacheEntry& entry)
{
    const auto age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry.timestamp).count();
    return age >= entry.time_interval;
}

bool CheckPath::both_sides_reachable(const std::wstring& source, const std::wstring& dest) const
{
    bool result = true;  // [CheckPath.md#reachability-rules]
    if (!side_reachable(source, true)) {
        log_info(translation::translate("source_dir_not_reachable") + source);
        result = false;
    }
    if (!side_reachable(dest, false)) {
        log_info(translation::translate("target_dir_not_reachable") + dest);
        result = false;
    }
    return result;
}

bool CheckPath::operator()(const std::wstring& source, const std::wstring& dest, [[maybe_unused]] bool ssh)  // [CheckPath.md#operator-ignores-ssh-flag]
{
    const std::wstring cache_key = source + L"|" + dest;
    std::lock_guard<std::mutex> lock(drive_check_cache_mutex_);
    const auto cached = drive_check_cache_.find(cache_key);
    const bool have_previous = cached != drive_check_cache_.end();
    if (have_previous && !expired(cached->second)) {
        log_debug(L"CheckPath: cached result for " + cache_key);
        return cached->second.result;
    }
    const bool result = both_sides_reachable(source, dest);
    const uint16_t previous_interval = have_previous ? cached->second.time_interval : BASE_INTERVAL;  // [CheckPath.md#interval-growth]
    const bool repeats = have_previous && cached->second.result == result;
    drive_check_cache_[cache_key] = {result, next_check_interval(previous_interval, repeats, result), std::chrono::steady_clock::now()};
    return result;
}

wstring root_name(const wstring& path)
{
    log_debug(L"root_name: " + path);
    if (path_contains_volume_name(path)) {
        const auto open_parenthesis_pos = path.find(L'(');
        const auto close_parenthesis_pos = path.find(L')');
        if (close_parenthesis_pos == wstring::npos)
            throw std::runtime_error(translation::translate_utf8("invalid_path") + wstring_to_string(path));
        return path.substr(open_parenthesis_pos + 1, close_parenthesis_pos - open_parenthesis_pos - 1);
    }
    const wstring retval = fs::path(path).root_name().wstring();
    log_debug(L"root_name returns: " + retval);
    return retval;
}

std::wstring without_volume_spec(const std::wstring& path)
{
    log_debug(L"without_volume_spec: " + path);
    if (!path_contains_volume_name(path))  // [CheckPath.md#volume-spec-stripping]
        return path;
    const auto close_parenthesis_pos = path.find(L')');
    if (close_parenthesis_pos == wstring::npos)
        return path;
    return root_name(path) + path.substr(close_parenthesis_pos + 1);
}

} // namespace AFS_Core
