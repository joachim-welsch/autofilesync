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
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "ViewActions.h"
#include "Config.h"
#include "GridCtrl.h"
#include "ListCtrl.h"
#include "TreeCtrl.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString FIRST = "guitest-erste";
const wxString SECOND = "guitest-zweite";

SyncItemInput sandbox_sync(const wxString& name, SyncItemType type = SyncItemType::File)
{
    SyncItemInput input{name, GuiTestSandbox::source_dir().string(), GuiTestSandbox::target_dir().string()};
    input.type = type;
    return input;
}

bool edit_and_delete_enabled_for(wxWindow& invoking_window)  // [ViewsTest.md#edit_and_delete_enabled_for]
{
    wxMenu menu;
    menu.Append(tul(id::EditSync), "&Editiere");
    menu.Append(tul(id::DeleteSync), L"&Lösche");
    menu.SetInvokingWindow(&invoking_window);
    menu.UpdateUI();
    menu.SetInvokingWindow(nullptr);
    return menu.IsEnabled(tul(id::EditSync)) && menu.IsEnabled(tul(id::DeleteSync));
}

std::vector<wxString> row_labels(const GridCtrl& grid)
{
    std::vector<wxString> labels;
    for (int row = 0; row < grid.GetNumberRows(); ++row)
        labels.push_back(grid.GetRowLabelValue(row));
    return labels;
}

}

TEST_F(GuiTest, AnsichtWechselZeigtGleicheSynchronisationen)  // [ViewsTest.md#AnsichtWechselZeigtGleicheSynchronisationen]
{
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(FIRST)));
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(SECOND, SyncItemType::Dir)));

    show_view(main_window(), id::View_Tree);
    ASSERT_NE(source_tree(main_window()), nullptr) << "Die Baumansicht erschien nicht.";
    EXPECT_EQ(sync_item_list(), nullptr) << "Die Liste blieb neben dem Baum bestehen.";
    EXPECT_TRUE(find_tree_item_for_path(*source_tree(main_window()), GuiTestSandbox::source_dir()).IsOk()) << "Die Quelle fehlt im Baum.";
    EXPECT_TRUE(find_tree_item_for_path(*target_tree(main_window()), GuiTestSandbox::target_dir()).IsOk()) << "Das Ziel fehlt im Baum.";

    show_view(main_window(), id::View_Grid);
    ASSERT_NE(grid_view(main_window()), nullptr) << "Die Tabellenansicht erschien nicht.";
    EXPECT_EQ(source_tree(main_window()), nullptr) << "Der Baum blieb neben der Tabelle bestehen.";
    EXPECT_EQ(row_labels(*grid_view(main_window())), (std::vector<wxString>{FIRST, SECOND}));
    EXPECT_EQ(grid_view(main_window())->GetCellValue(0, 1), GuiTestSandbox::source_dir().string());

    show_view(main_window(), id::View_List);
    ASSERT_NE(sync_item_list(), nullptr) << "Die Listenansicht erschien nicht.";
    EXPECT_EQ(grid_view(main_window()), nullptr) << "Die Tabelle blieb neben der Liste bestehen.";
    EXPECT_TRUE(list_shows(FIRST.ToStdWstring()));
    EXPECT_TRUE(list_shows(SECOND.ToStdWstring()));
}

TEST_F(GuiTest, KontextmenueInListeAktivWennZeileAusgewaehlt)  // [ViewsTest.md#context-menus]
{
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(FIRST)));
    ListCtrl* list = sync_item_list();
    ASSERT_NE(list, nullptr);
    wxWindow* list_window = list->GetParent();
    list->SetItemState(0, 0, wxLIST_STATE_SELECTED);
    pump();
    EXPECT_FALSE(edit_and_delete_enabled_for(*list_window)) << "Das Kontextmenü ist ohne ausgewählte Zeile bedienbar.";

    ASSERT_TRUE(click_on(*list, 0));

    EXPECT_TRUE(edit_and_delete_enabled_for(*list_window)) << "Das Kontextmenü bleibt mit ausgewählter Zeile gesperrt.";
}

TEST_F(GuiTest, KontextmenueInTabelleAktivWennZeileAusgewaehlt)  // [ViewsTest.md#context-menus]
{
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(FIRST)));
    show_view(main_window(), id::View_Grid);
    GridCtrl* grid = grid_view(main_window());
    ASSERT_NE(grid, nullptr);
    wxWindow* grid_window = grid->GetParent();

    grid->SetGridCursor(0, 1);
    pump();
    EXPECT_TRUE(edit_and_delete_enabled_for(*grid_window)) << "Das Kontextmenü bleibt mit Cursor auf einer Zeile gesperrt.";
}

TEST_F(GuiTest, AktivPerKlickInTabelleAendertKonfiguration)  // [ViewsTest.md#grid-editing]
{
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(FIRST)));
    show_view(main_window(), id::View_Grid);
    GridCtrl* grid = grid_view(main_window());
    ASSERT_NE(grid, nullptr);
    ASSERT_EQ(grid->GetCellValue(0, 0), "1") << "Eine neue Synchronisation ist in der Tabelle nicht aktiv.";

    ASSERT_TRUE(change_grid_cell(*grid, 0, 0, ""));

    const auto item = Config::instance().find_by_name(FIRST.ToStdWstring());
    ASSERT_TRUE(item.has_value());
    EXPECT_FALSE(item->enabled()) << "Das Abwählen von Aktiv kam nicht in der Konfiguration an.";
}

TEST_F(GuiTest, LeereQuelleInTabelleWirdAbgelehntUndZurueckgesetzt)  // [ViewsTest.md#grid-editing]
{
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(FIRST)));
    show_view(main_window(), id::View_Grid);
    GridCtrl* grid = grid_view(main_window());
    ASSERT_NE(grid, nullptr);
    ModalDialogWatcher::instance().answer(translation::translate("error"), wxID_OK);

    ASSERT_TRUE(change_grid_cell(*grid, 0, 1, ""));

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Die leere Quelle wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("source_target_required"));
    EXPECT_EQ(grid->GetCellValue(0, 1), GuiTestSandbox::source_dir().string()) << "Die Tabelle zeigt die leere Quelle weiter an.";
    const auto item = Config::instance().find_by_name(FIRST.ToStdWstring());
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->source(), GuiTestSandbox::source_dir().wstring()) << "Die leere Quelle kam in die Konfiguration.";
}

TEST_F(GuiTest, BaumZeigtDateiUndOrdnerSymbole)  // [ViewsTest.md#BaumZeigtDateiUndOrdnerSymbole]
{
    SyncItemInput file_sync = sandbox_sync(FIRST);
    file_sync.source = (GuiTestSandbox::source_dir() / "beispiel.txt").string();
    ASSERT_TRUE(add_sync_item(main_window(), file_sync));
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync(SECOND, SyncItemType::Dir)));
    show_view(main_window(), id::View_Tree);
    wxTreeCtrl* tree = source_tree(main_window());
    ASSERT_NE(tree, nullptr);

    const wxTreeItemId file = find_tree_item_for_path(*tree, GuiTestSandbox::source_dir() / "beispiel.txt");
    const wxTreeItemId folder = find_tree_item_for_path(*tree, GuiTestSandbox::source_dir());
    ASSERT_TRUE(file.IsOk()) << "Die Datei fehlt im Baum.";
    ASSERT_TRUE(folder.IsOk()) << "Der Ordner fehlt im Baum.";
    EXPECT_EQ(tree->GetItemImage(file), static_cast<int>(TreeCtrlIcon::File));
    EXPECT_NE(tree->GetItemImage(folder), static_cast<int>(TreeCtrlIcon::File)) << "Der Ordner trägt das Dateisymbol.";
}

}
