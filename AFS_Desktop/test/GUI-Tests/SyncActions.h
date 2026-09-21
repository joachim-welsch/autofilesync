#pragma once

class MainWindow;

namespace guitest {  // [SyncActions.md#SyncActions]

constexpr int SYNC_TIMEOUT_MILLISECONDS = 30000;

bool waitUntilSyncFinished(MainWindow& window, int timeoutMilliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#waitUntilSyncFinished]
bool syncOnce(MainWindow& window, int timeoutMilliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#syncOnce]
bool syncMenuItemEnabled(MainWindow& window, int menuItemId);

}
