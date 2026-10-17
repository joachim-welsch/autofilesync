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
#include "SyncArbiter.h"
#include <chrono>
#include <thread>
#include <Logging.h>
#include <Translation.h>
#include <string>
#include <filesystem>
#ifdef WIN32
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <sys/file.h>
    #include <unistd.h>
    #include <cerrno>
#endif

namespace AFS_Core {

namespace {
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(200);
constexpr auto FOREGROUND_WAIT_LIMIT = std::chrono::seconds(60);

#ifdef _TEST
constexpr bool TEST_SCOPE = true;  // [SyncArbiter.md#TEST_SCOPE]
#else
constexpr bool TEST_SCOPE = false;
#endif
std::atomic_bool private_scope = TEST_SCOPE;

#ifdef WIN32
std::wstring event_name(SyncRole role)
{
    const std::wstring name = role == SyncRole::Foreground ? L"Global\\AutoFileSyncForegroundSync" : L"Global\\AutoFileSyncBackgroundSync";
    return private_scope ? name + L"Test" : name;
}
#else
std::string lock_path(SyncRole role)
{
    const std::string suffix = role == SyncRole::Foreground ? "foreground" : "background";
    const std::string dir = private_scope ? std::filesystem::temp_directory_path().string() : "/tmp";  // [SyncArbiter.md#TEST_SCOPE]
    return dir + "/autofilesync-" + std::to_string(getuid()) + (private_scope ? "-test-" : "-") + suffix + ".lock";  // [SyncArbiter.md#per-user-lock-path]
}
#endif

SyncRole other_role(SyncRole role)
{
    return role == SyncRole::Foreground ? SyncRole::Background : SyncRole::Foreground;
}
}

void SyncArbiter::publish_active()
{
#ifdef WIN32
    active_event_ = CreateEventW(nullptr, TRUE, TRUE, event_name(role_).c_str());
    if (active_event_ == nullptr)
        log_warning(translation::translate("arbiter_announce_failed"), std::to_wstring(GetLastError()));
#else
    active_lock_fd_ = open(lock_path(role_).c_str(), O_RDWR | O_CREAT, 0600);
    if (active_lock_fd_ < 0) {
        log_warning(translation::translate("arbiter_lock_file_failed"), std::to_wstring(errno));
        return;
    }
    if (flock(active_lock_fd_, LOCK_EX | LOCK_NB) != 0)
        log_warning(translation::translate("arbiter_own_lock_taken"));
#endif
}

void SyncArbiter::withdraw_active()
{
#ifdef WIN32
    if (active_event_ != nullptr) {
        CloseHandle(active_event_);
        active_event_ = nullptr;
    }
#else
    if (active_lock_fd_ >= 0) {
        flock(active_lock_fd_, LOCK_UN);
        close(active_lock_fd_);
        active_lock_fd_ = -1;
    }
#endif
}

bool SyncArbiter::other_side_active() const
{
#ifdef WIN32
    HANDLE other = OpenEventW(SYNCHRONIZE, FALSE, event_name(other_role(role_)).c_str());
    if (other != nullptr) {
        CloseHandle(other);
        return true;
    }
    return GetLastError() != ERROR_FILE_NOT_FOUND;
#else
    const int other = open(lock_path(other_role(role_)).c_str(), O_RDWR | O_CREAT, 0600);
    if (other < 0)
        return false;
    const bool free = flock(other, LOCK_EX | LOCK_NB) == 0;
    if (free)
        flock(other, LOCK_UN);
    close(other);
    return !free;
#endif
}

void SyncArbiter::use_private_scope()
{
    private_scope = true;
}

bool SyncArbiter::acquire(const std::atomic_bool& abort)
{
    if (held_)
        return true;
    if (role_ == SyncRole::Background) {
        while (!abort.load()) {
            publish_active();
            if (!other_side_active()) {
                held_ = true;
                return true;
            }
            withdraw_active();
            std::this_thread::sleep_for(POLL_INTERVAL);
        }
        return false;
    }
    publish_active();
    const auto deadline = std::chrono::steady_clock::now() + FOREGROUND_WAIT_LIMIT;
    while (other_side_active()) {
        if (abort.load()) {
            withdraw_active();
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            log_warning(translation::translate("arbiter_service_not_releasing"));
            break;
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    held_ = true;
    return true;
}

void SyncArbiter::release()
{
    if (!held_)
        return;
    withdraw_active();
    held_ = false;
}

SyncArbiter::~SyncArbiter()
{
    release();
}

} // namespace AFS_Core
