/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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

#include "SyncItemDialogDriver.h"

class MainWindow;
class ProgressDialog;

namespace guitest {  // [SyncActions.md#SyncActions]

constexpr int SYNC_TIMEOUT_MILLISECONDS = 30000;

bool addSyncItem(MainWindow& window, const SyncItemInput& input);  // [SyncActions.md#addSyncItem]
bool waitUntilSyncFinished(MainWindow& window, int timeoutMilliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#waitUntilSyncFinished]
bool syncOnce(MainWindow& window, int timeoutMilliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#syncOnce]
bool syncMenuItemEnabled(MainWindow& window, int menuItemId);
ProgressDialog* progressDialog();  // [SyncActions.md#progressDialog]

}
