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

const wxString sync_item_name = "guitest-baumansicht-gegenseite";

}

TEST_F(GuiTest, LinksclickImBaumZeigtDenPfadAufDerAnderenSeite)  // [TreeViewCrossSelectionTest.md#LinksclickImBaumZeigtDenPfadAufDerAnderenSeite]
{
    const SyncItemInput input{sync_item_name,
        GuiTestSandbox::source_dir().string(), GuiTestSandbox::target_dir().string()};
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fill_sync_item_dialog(input));
    activate_menu_item(main_window(), tul(id::File_new_Synchronization));
    pump();
    ASSERT_TRUE(Config::instance().find_by_name(sync_item_name.ToStdWstring()).has_value())
        << "Die Synchronisation wurde nicht angelegt.";

    activate_menu_item(main_window(), tul(id::View_Tree));
    pump();

    wxWindow* source_canvas = wxWindow::FindWindowByName("source", &main_window());
    wxWindow* target_canvas = wxWindow::FindWindowByName("target", &main_window());
    ASSERT_NE(source_canvas, nullptr) << "Das Quell-Panel der Baumansicht wurde nicht gefunden.";
    ASSERT_NE(target_canvas, nullptr) << "Das Ziel-Panel der Baumansicht wurde nicht gefunden.";
    wxTreeCtrl* source_tree = dynamic_cast<wxTreeCtrl*>(find(*source_canvas, ID_SRC_TREE_CTRL));
    wxTreeCtrl* target_tree = dynamic_cast<wxTreeCtrl*>(find(*target_canvas, ID_SRC_TREE_CTRL));
    ASSERT_NE(source_tree, nullptr) << "Der Baum des Quell-Panels wurde nicht gefunden.";
    ASSERT_NE(target_tree, nullptr) << "Der Baum des Ziel-Panels wurde nicht gefunden.";

    const wxTreeItemId source_item = find_tree_item_for_path(*source_tree, GuiTestSandbox::source_dir());
    const wxTreeItemId target_item = find_tree_item_for_path(*target_tree, GuiTestSandbox::target_dir());
    ASSERT_TRUE(source_item.IsOk()) << "Der Quellordner der Synchronisation fehlt im Baum.";
    ASSERT_TRUE(target_item.IsOk()) << "Der Zielordner der Synchronisation fehlt im Baum.";
    source_tree->EnsureVisible(source_item);
    pump();

    ASSERT_TRUE(click_on(*source_tree, source_item)) << "Der Quellordner konnte nicht angeklickt werden.";
    EXPECT_EQ(target_tree->GetSelection(), target_item)
        << "Der Klick auf den Quellordner haette den zugehoerigen Zielordner auswaehlen sollen.";

    source_tree->SelectItem(source_tree->GetRootItem(), false);  // and the other way round: clicking the target side reveals the source
    target_tree->EnsureVisible(target_item);
    pump();
    ASSERT_TRUE(click_on(*target_tree, target_item)) << "Der Zielordner konnte nicht angeklickt werden.";
    EXPECT_EQ(source_tree->GetSelection(), source_item)
        << "Der Klick auf den Zielordner haette den zugehoerigen Quellordner auswaehlen sollen.";
}

}
