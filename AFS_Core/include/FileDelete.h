#pragma once
#include <string>

namespace AFS_Core {

// Deletes according to the configured DeleteMode: permanently, or by moving the
// file into the recycle bin under a timestamped name. Every deletion of data
// that belongs to the user goes through here; our own temporary files (backups,
// half-copied targets, the config temp file) are removed directly, they have
// nothing to recover.
bool delete_file(const std::wstring& pathname);

} // namespace AFS_Core
