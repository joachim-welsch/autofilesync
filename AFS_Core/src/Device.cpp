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
#include "Device.h"
#include <algorithm>
#include <cctype>

namespace AFS_Core {

bool valid_device_id(const std::string& id)  // [Device.md#valid_device_id]
{
    constexpr size_t LENGTH = 32;
    return id.size() == LENGTH && std::ranges::all_of(id, [](unsigned char c) { return std::isxdigit(c) && !std::isupper(c); });
}

} // namespace AFS_Core
