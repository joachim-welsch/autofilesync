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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>
#include "SyncItem.h"

namespace AFS_Core {

enum class RestorableKind { Deleted, EarlierVersion };

struct Restorable {  // [Restore.md#Restorable]
    RestorableKind kind = RestorableKind::Deleted;
    std::filesystem::path stored;
    std::filesystem::path home;
    std::chrono::system_clock::time_point time;
    std::uintmax_t size = 0;
};

using Restorables = std::vector<Restorable>;

namespace restore {  // [Restore.md#restore]
Restorables find(const SyncItems& items, const std::atomic<bool>& stop);  // [Restore.md#find]
bool place_known(const Restorable& file);  // [Restore.md#place_known]
std::optional<std::filesystem::path> put_back(const Restorable& file, const std::filesystem::path& directory);  // [Restore.md#put_back]
}

} // namespace AFS_Core
