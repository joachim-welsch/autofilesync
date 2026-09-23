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
#include "Canvas.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString syncItemName = "guitest-baumansicht";

bool editAndDeleteEnabledFor(wxWindow& invokingCanvas)  // [TreeViewContextMenuTest.md#editAndDeleteEnabledFor]
{
    wxMenu menu;
    menu.Append(tul(id::EditSync), "&Editiere");
    menu.Append(tul(id::DeleteSync), L"&Lösche");
    menu.SetInvokingWindow(&invokingCanvas);
    menu.UpdateUI();
    menu.SetInvokingWindow(nullptr);
    return menu.IsEnabled(tul(id::EditSync)) && menu.IsEnabled(tul(id::DeleteSync));
}

}

TEST_F(GuiTest, KontextmenueInBaumansichtAktivWennSyncItemAusgewaehlt)
{
    const SyncItemInput input{syncItemName,
        GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));
    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();
    ASSERT_TRUE(Config::instance().find_by_name(syncItemName.ToStdWstring()).has_value())
        << "Die Synchronisation wurde nicht angelegt.";

    activateMenuItem(mainWindow(), tul(id::View_Tree));
    pump();

    wxWindow* sourceCanvas = wxWindow::FindWindowByName("source", &mainWindow());
    ASSERT_NE(sourceCanvas, nullptr) << "Das Quell-Panel der Baumansicht wurde nicht gefunden.";
    wxTreeCtrl* tree = dynamic_cast<wxTreeCtrl*>(find(*sourceCanvas, ID_SRC_TREE_CTRL));
    ASSERT_NE(tree, nullptr) << "Der Baum des Quell-Panels wurde nicht gefunden.";

    tree->SelectItem(tree->GetRootItem());  // nothing belonging to the SyncItem selected yet: still grayed out
    pump();
    EXPECT_FALSE(editAndDeleteEnabledFor(*sourceCanvas))
        << "Das Kontextmenue sollte ohne Auswahl eines SyncItem deaktiviert sein.";

    const wxTreeItemId sourceItem = findTreeItemForPath(*tree, GuiTestSandbox::sourceDir());
    ASSERT_TRUE(sourceItem.IsOk()) << "Der Quellordner der Synchronisation fehlt im Baum.";
    tree->SelectItem(sourceItem);
    pump();
    EXPECT_TRUE(editAndDeleteEnabledFor(*sourceCanvas))
        << "Das Kontextmenue sollte bedienbar sein, wenn der Quellordner des SyncItem ausgewaehlt ist.";
}

}
