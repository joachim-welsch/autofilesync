#include "settings.h"

void Settings::reset()
{
    startMinimized_    = false;
    autoStart_         = false;
    startSyncAtStart_  = false;
    syncCycle_         = 1;
}
