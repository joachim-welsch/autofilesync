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
#include "id.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString FIRST = "guitest-erste";
const wxString SECOND = "guitest-zweite";

SyncItemInput sandboxSync(const wxString& name, SyncItemType type = SyncItemType::File)
{
    SyncItemInput input{name, GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
    input.type = type;
    return input;
}

bool editAndDeleteEnabledFor(wxWindow& invokingWindow)  // [ViewsTest.md#editAndDeleteEnabledFor]
{
    wxMenu menu;
    menu.Append(tul(id::EditSync), "&Editiere");
    menu.Append(tul(id::DeleteSync), L"&Lösche");
    menu.SetInvokingWindow(&invokingWindow);
    menu.UpdateUI();
    menu.SetInvokingWindow(nullptr);
    return menu.IsEnabled(tul(id::EditSync)) && menu.IsEnabled(tul(id::DeleteSync));
}

std::vector<wxString> rowLabels(const GridCtrl& grid)
{
    std::vector<wxString> labels;
    for (int row = 0; row < grid.GetNumberRows(); ++row)
        labels.push_back(grid.GetRowLabelValue(row));
    return labels;
}

}

TEST_F(GuiTest, AnsichtWechselZeigtGleicheSynchronisationen)  // [ViewsTest.md#AnsichtWechselZeigtGleicheSynchronisationen]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(FIRST)));
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(SECOND, SyncItemType::Dir)));
    const ListViewRestored restored(mainWindow());

    showView(mainWindow(), id::View_Tree);
    ASSERT_NE(sourceTree(mainWindow()), nullptr) << "Die Baumansicht erschien nicht.";
    EXPECT_EQ(syncItemList(), nullptr) << "Die Liste blieb neben dem Baum bestehen.";
    EXPECT_TRUE(findTreeItemForPath(*sourceTree(mainWindow()), GuiTestSandbox::sourceDir()).IsOk()) << "Die Quelle fehlt im Baum.";
    EXPECT_TRUE(findTreeItemForPath(*targetTree(mainWindow()), GuiTestSandbox::targetDir()).IsOk()) << "Das Ziel fehlt im Baum.";

    showView(mainWindow(), id::View_Grid);
    ASSERT_NE(gridView(mainWindow()), nullptr) << "Die Tabellenansicht erschien nicht.";
    EXPECT_EQ(sourceTree(mainWindow()), nullptr) << "Der Baum blieb neben der Tabelle bestehen.";
    EXPECT_EQ(rowLabels(*gridView(mainWindow())), (std::vector<wxString>{FIRST, SECOND}));
    EXPECT_EQ(gridView(mainWindow())->GetCellValue(0, 1), GuiTestSandbox::sourceDir().string());

    showView(mainWindow(), id::View_List);
    ASSERT_NE(syncItemList(), nullptr) << "Die Listenansicht erschien nicht.";
    EXPECT_EQ(gridView(mainWindow()), nullptr) << "Die Tabelle blieb neben der Liste bestehen.";
    EXPECT_TRUE(listShows(FIRST.ToStdWstring()));
    EXPECT_TRUE(listShows(SECOND.ToStdWstring()));
}

TEST_F(GuiTest, KontextmenueInListeAktivWennZeileAusgewaehlt)  // [ViewsTest.md#context-menus]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(FIRST)));
    ListCtrl* list = syncItemList();
    ASSERT_NE(list, nullptr);
    wxWindow* listWindow = list->GetParent();
    list->SetItemState(0, 0, wxLIST_STATE_SELECTED);
    pump();
    EXPECT_FALSE(editAndDeleteEnabledFor(*listWindow)) << "Das Kontextmenü ist ohne ausgewählte Zeile bedienbar.";

    ASSERT_TRUE(clickOn(*list, 0));

    EXPECT_TRUE(editAndDeleteEnabledFor(*listWindow)) << "Das Kontextmenü bleibt mit ausgewählter Zeile gesperrt.";
}

TEST_F(GuiTest, KontextmenueInTabelleAktivWennZeileAusgewaehlt)  // [ViewsTest.md#context-menus]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(FIRST)));
    const ListViewRestored restored(mainWindow());
    showView(mainWindow(), id::View_Grid);
    GridCtrl* grid = gridView(mainWindow());
    ASSERT_NE(grid, nullptr);
    wxWindow* gridWindow = grid->GetParent();

    grid->SetGridCursor(0, 1);
    pump();
    EXPECT_TRUE(editAndDeleteEnabledFor(*gridWindow)) << "Das Kontextmenü bleibt mit Cursor auf einer Zeile gesperrt.";
}

TEST_F(GuiTest, AktivPerKlickInTabelleAendertKonfiguration)  // [ViewsTest.md#grid-editing]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(FIRST)));
    const ListViewRestored restored(mainWindow());
    showView(mainWindow(), id::View_Grid);
    GridCtrl* grid = gridView(mainWindow());
    ASSERT_NE(grid, nullptr);
    ASSERT_EQ(grid->GetCellValue(0, 0), "1") << "Eine neue Synchronisation ist in der Tabelle nicht aktiv.";

    ASSERT_TRUE(changeGridCell(*grid, 0, 0, ""));

    const auto item = Config::instance().find_by_name(FIRST.ToStdWstring());
    ASSERT_TRUE(item.has_value());
    EXPECT_FALSE(item->enabled()) << "Das Abwählen von Aktiv kam nicht in der Konfiguration an.";
}

TEST_F(GuiTest, LeereQuelleInTabelleWirdAbgelehntUndZurueckgesetzt)  // [ViewsTest.md#grid-editing]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(FIRST)));
    const ListViewRestored restored(mainWindow());
    showView(mainWindow(), id::View_Grid);
    GridCtrl* grid = gridView(mainWindow());
    ASSERT_NE(grid, nullptr);
    ModalDialogWatcher::instance().answer(_("Fehler"), wxID_OK);

    ASSERT_TRUE(changeGridCell(*grid, 0, 1, ""));

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Die leere Quelle wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), _(L"Quelle und Ziel dürfen nicht leer sein."));
    EXPECT_EQ(grid->GetCellValue(0, 1), GuiTestSandbox::sourceDir().string()) << "Die Tabelle zeigt die leere Quelle weiter an.";
    const auto item = Config::instance().find_by_name(FIRST.ToStdWstring());
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->source(), GuiTestSandbox::sourceDir().wstring()) << "Die leere Quelle kam in die Konfiguration.";
}

TEST_F(GuiTest, BaumZeigtDateiUndOrdnerSymbole)  // [ViewsTest.md#BaumZeigtDateiUndOrdnerSymbole]
{
    SyncItemInput fileSync = sandboxSync(FIRST);
    fileSync.source = (GuiTestSandbox::sourceDir() / "beispiel.txt").string();
    ASSERT_TRUE(addSyncItem(mainWindow(), fileSync));
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync(SECOND, SyncItemType::Dir)));
    const ListViewRestored restored(mainWindow());
    showView(mainWindow(), id::View_Tree);
    wxTreeCtrl* tree = sourceTree(mainWindow());
    ASSERT_NE(tree, nullptr);

    const wxTreeItemId file = findTreeItemForPath(*tree, GuiTestSandbox::sourceDir() / "beispiel.txt");
    const wxTreeItemId folder = findTreeItemForPath(*tree, GuiTestSandbox::sourceDir());
    ASSERT_TRUE(file.IsOk()) << "Die Datei fehlt im Baum.";
    ASSERT_TRUE(folder.IsOk()) << "Der Ordner fehlt im Baum.";
    EXPECT_EQ(tree->GetItemImage(file), static_cast<int>(TreeCtrlIcon::File));
    EXPECT_NE(tree->GetItemImage(folder), static_cast<int>(TreeCtrlIcon::File)) << "Der Ordner trägt das Dateisymbol.";
}

}
