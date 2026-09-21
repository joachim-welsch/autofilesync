#include "SyncActions.h"
#include "EventPump.h"
#include "UserActions.h"
#include "MainWindow.h"
#include "Synchronize.h"
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

bool waitUntilSyncFinished(MainWindow& window, int timeoutMilliseconds)
{
    return waitUntil([&window] {  // [SyncActions.md#waitUntilSyncFinished]
        return window.syncInterface().actSyncType() == SyncType::None && !Synchronize::has_instance();
    }, timeoutMilliseconds);
}

bool syncOnce(MainWindow& window, int timeoutMilliseconds)
{
    activateMenuItem(window, tul(id::SyncOnce));
    pump();
    return waitUntilSyncFinished(window, timeoutMilliseconds);
}

bool syncMenuItemEnabled(MainWindow& window, int menuItemId)
{
    return window.menu().syncMenu()->IsEnabled(menuItemId);
}

}
