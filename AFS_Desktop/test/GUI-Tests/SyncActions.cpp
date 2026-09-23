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
#include "SyncActions.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "Config.h"
#include "MainWindow.h"
#include "ProgressDialog.h"
#include "Synchronize.h"
#include "TextResources.h"
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

bool addSyncItem(MainWindow& window, const SyncItemInput& input)
{
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));
    activateMenuItem(window, tul(id::File_new_Synchronization));
    pump();
    return Config::instance().find_by_name(input.name.ToStdWstring()).has_value();
}

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

ProgressDialog* progressDialog()
{
    for (wxWindow* window : wxTopLevelWindows) {
        if (auto* dialog = dynamic_cast<ProgressDialog*>(window); dialog != nullptr && dialog->IsShown())
            return dialog;
    }
    return nullptr;
}

}
