/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
