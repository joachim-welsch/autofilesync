#include "pch.h"
#include "settings.h"

void Settings::reset()
{
    startMinimized_    = false;
    minimizeInsteadOfClose_ = false;
    autoStart_ = false;
    startSyncAtStart_  = false;
    deleteMode_ = DeleteMode::RecycleBin;
    backupProofPath_.clear();
}
