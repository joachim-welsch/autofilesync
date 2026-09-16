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
