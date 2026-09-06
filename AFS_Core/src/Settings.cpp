#include "pch.h"
#include "Settings.h"

namespace AFS_Core {

void Settings::reset()
{
    startMinimized_    = false;
    minimizeInsteadOfClose_ = false;
    autoStart_ = false;
    startSyncAtStart_  = false;
    deleteMode_ = DeleteMode::RecycleBin;
    backupProofPath_.clear();
}

} // namespace AFS_Core
