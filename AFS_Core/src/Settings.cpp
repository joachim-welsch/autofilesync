#include "pch.h"
#include "Settings.h"

namespace AFS_Core {

void Settings::reset()
{
    start_minimized_    = false;
    minimize_instead_of_close_ = false;
    auto_start_ = false;
    start_sync_at_start_  = false;
    delete_mode_ = DeleteMode::RecycleBin;
    backup_proof_path_.clear();
}

} // namespace AFS_Core
