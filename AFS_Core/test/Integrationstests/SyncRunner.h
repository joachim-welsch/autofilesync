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

#include <cstddef>
#include "SftpConnection.h"
#include "SyncDir.h"
#include "SyncFile.h"
#include "SyncItem.h"
#include "SyncTree.h"

namespace tu {

inline std::size_t run_sync(const AFS_Core::SyncItem& si, AFS_Core::SyncState* state)  // [SyncRunner.md#run_sync]
{
	using namespace AFS_Core;
	[[maybe_unused]] SshSessionScope ssh_sessions;  // [SyncRunner.md#ssh-sessions]
	std::size_t copy_count = 0;
	switch (si.sync_item_type()) {
	case SyncItemType::Tree: { SyncTree tree(si, copy_count, false, state); tree(); break; }
	case SyncItemType::Dir: { SyncDir dir(si, copy_count, false, state); dir(); break; }
	default: { SyncFile file(si, copy_count, false, false, state); file(); break; }
	}
	return copy_count;
}
inline std::size_t run_sync(const AFS_Core::SyncItem& si)
{
	return run_sync(si, nullptr);
}
inline std::size_t run_sync(const AFS_Core::SyncItem& si, AFS_Core::SyncState& state)  // [SyncRunner.md#run_sync-with-state]
{
	return run_sync(si, &state);
}

} // namespace tu
