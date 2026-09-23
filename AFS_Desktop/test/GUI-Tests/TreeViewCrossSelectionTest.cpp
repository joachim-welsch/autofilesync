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

const wxString syncItemName = "guitest-baumansicht-gegenseite";

}

TEST_F(GuiTest, LinksclickImBaumZeigtDenPfadAufDerAnderenSeite)  // [TreeViewCrossSelectionTest.md#LinksclickImBaumZeigtDenPfadAufDerAnderenSeite]
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
    wxWindow* targetCanvas = wxWindow::FindWindowByName("target", &mainWindow());
    ASSERT_NE(sourceCanvas, nullptr) << "Das Quell-Panel der Baumansicht wurde nicht gefunden.";
    ASSERT_NE(targetCanvas, nullptr) << "Das Ziel-Panel der Baumansicht wurde nicht gefunden.";
    wxTreeCtrl* sourceTree = dynamic_cast<wxTreeCtrl*>(find(*sourceCanvas, ID_SRC_TREE_CTRL));
    wxTreeCtrl* targetTree = dynamic_cast<wxTreeCtrl*>(find(*targetCanvas, ID_SRC_TREE_CTRL));
    ASSERT_NE(sourceTree, nullptr) << "Der Baum des Quell-Panels wurde nicht gefunden.";
    ASSERT_NE(targetTree, nullptr) << "Der Baum des Ziel-Panels wurde nicht gefunden.";

    const wxTreeItemId sourceItem = findTreeItemForPath(*sourceTree, GuiTestSandbox::sourceDir());
    const wxTreeItemId targetItem = findTreeItemForPath(*targetTree, GuiTestSandbox::targetDir());
    ASSERT_TRUE(sourceItem.IsOk()) << "Der Quellordner der Synchronisation fehlt im Baum.";
    ASSERT_TRUE(targetItem.IsOk()) << "Der Zielordner der Synchronisation fehlt im Baum.";
    sourceTree->EnsureVisible(sourceItem);
    pump();

    ASSERT_TRUE(clickOn(*sourceTree, sourceItem)) << "Der Quellordner konnte nicht angeklickt werden.";
    EXPECT_EQ(targetTree->GetSelection(), targetItem)
        << "Der Klick auf den Quellordner haette den zugehoerigen Zielordner auswaehlen sollen.";

    sourceTree->SelectItem(sourceTree->GetRootItem(), false);  // and the other way round: clicking the target side reveals the source
    targetTree->EnsureVisible(targetItem);
    pump();
    ASSERT_TRUE(clickOn(*targetTree, targetItem)) << "Der Zielordner konnte nicht angeklickt werden.";
    EXPECT_EQ(sourceTree->GetSelection(), sourceItem)
        << "Der Klick auf den Zielordner haette den zugehoerigen Quellordner auswaehlen sollen.";
}

}
