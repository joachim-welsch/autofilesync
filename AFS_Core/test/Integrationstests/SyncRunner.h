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

#include <cstddef>
#include "SyncDir.h"
#include "SyncFile.h"
#include "SyncItem.h"
#include "SyncTree.h"

namespace tu {

inline std::size_t run_sync(const AFS_Core::SyncItem& si)  // [SyncRunner.md#run_sync]
{
	using namespace AFS_Core;
	std::size_t copy_count = 0;
	switch (si.sync_item_type()) {
	case SyncItemType::Tree: { SyncTree tree(si, copy_count, false); tree(); break; }
	case SyncItemType::Dir: { SyncDir dir(si, copy_count, false); dir(); break; }
	default: { SyncFile file(si, copy_count, false); file(); break; }
	}
	return copy_count;
}

} // namespace tu
