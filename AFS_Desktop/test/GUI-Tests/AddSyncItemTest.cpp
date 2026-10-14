/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
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

const wxString sync_item_name = "guitest-sicherung";  // only characters wxUIActionSimulator can produce on a plain keyboard layout

}

TEST_F(GuiTest, AngelegteSynchronisationIstInKonfigurationUndListeVorhanden)
{
    const SyncItemInput input{sync_item_name,
        GuiTestSandbox::source_dir().string(), GuiTestSandbox::target_dir().string()};
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fill_sync_item_dialog(input));

    activate_menu_item(main_window(), tul(id::File_new_Synchronization));
    pump();

    const auto created = Config::instance().find_by_name(sync_item_name.ToStdWstring());
    ASSERT_TRUE(created.has_value()) << "Die Synchronisation wurde nicht angelegt.";
    EXPECT_EQ(created->source(), input.source.ToStdWstring());
    EXPECT_EQ(created->target(), input.target.ToStdWstring());
    EXPECT_TRUE(list_shows(sync_item_name.ToStdWstring())) << "Die Synchronisation fehlt in der Liste.";
}

}
