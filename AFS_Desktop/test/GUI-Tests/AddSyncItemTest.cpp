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
#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString syncItemName = "guitest-sicherung";  // only characters wxUIActionSimulator can produce on a plain keyboard layout

}

TEST_F(GuiTest, AngelegteSynchronisationIstInKonfigurationUndListeVorhanden)
{
    const SyncItemInput input{syncItemName,
        GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));

    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();

    const auto created = Config::instance().find_by_name(syncItemName.ToStdWstring());
    ASSERT_TRUE(created.has_value()) << "Die Synchronisation wurde nicht angelegt.";
    EXPECT_EQ(created->source(), input.source.ToStdWstring());
    EXPECT_EQ(created->target(), input.target.ToStdWstring());
    EXPECT_TRUE(listShows(syncItemName.ToStdWstring())) << "Die Synchronisation fehlt in der Liste.";
}

}
