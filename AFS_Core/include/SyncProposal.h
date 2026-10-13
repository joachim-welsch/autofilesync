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
#include <optional>
#include <string>
#include "SyncItem.h"

namespace AFS_Core {

namespace sync_proposal {  // [SyncProposal.md#sync_proposal]
std::wstring instruction();  // [SyncProposal.md#instruction]
std::optional<SyncItem> parse(const std::wstring& answer);  // [SyncProposal.md#parse]
std::wstring without_block(const std::wstring& answer);  // [SyncProposal.md#without_block]
}

} // namespace AFS_Core
