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

#include <atomic>

namespace AFS_Core {

enum class SyncRole { Foreground, Background };  // [SyncArbiter.md#SyncArbiter]

class SyncArbiter {
public:
    explicit SyncArbiter(SyncRole role): role_(role) {}
    SyncArbiter(const SyncArbiter&) = delete;
    SyncArbiter& operator=(const SyncArbiter&) = delete;
    ~SyncArbiter();
    bool acquire(const std::atomic_bool& abort);  // [SyncArbiter.md#acquire]
    void release();
private:
    bool other_side_active() const;
    void publish_active();
    void withdraw_active();

    SyncRole role_;
    bool held_ = false;
#ifdef WIN32
    void* active_event_ = nullptr;  // [SyncArbiter.md#active_event_]
#else
    int active_lock_fd_ = -1;
#endif
};

} // namespace AFS_Core
