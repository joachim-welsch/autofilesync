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
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString ITEM_NAME = "guitest-verwalten";
wxString confirm_delete()  // translated at the call, once the catalogue is bound
{
    return translation::translate("confirm_delete_title");
}

SyncItemInput sandboxSync(const wxString& name = ITEM_NAME)
{
    return SyncItemInput{name, GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
}

SyncItemInput withNewSource(const SyncItemInput& input)
{
    SyncItemInput edited = input;
    edited.source = (GuiTestSandbox::root() / "neue-quelle").string();
    return edited;
}

long listRowOf(ListCtrl& list, const wxString& name)
{
    for (long row = 0; row < list.GetItemCount(); ++row)
        if (list.GetItemText(row) == name)
            return row;
    return -1;
}

std::wstring sourceOf(const wxString& name)
{
    const auto item = Config::instance().find_by_name(name.ToStdWstring());
    return item ? item->source() : L"<fehlt>";
}

}

TEST_F(GuiTest, SynchronisationBearbeitenUeberMenueUebernimmtNeueQuelle)  // [SyncItemManagementTest.md#edit-routes]
{
    const SyncItemInput input = sandboxSync();
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    ListCtrl* list = syncItemList();
    ASSERT_NE(list, nullptr);
    ASSERT_TRUE(clickOn(*list, listRowOf(*list, ITEM_NAME))) << "Die Zeile ließ sich nicht anklicken.";

    const SyncItemInput edited = withNewSource(input);
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), fillSyncItemDialog(edited));
    activateMenuItem(mainWindow(), tul(id::EditSync));
    pump();

    EXPECT_EQ(sourceOf(ITEM_NAME), edited.source.ToStdWstring()) << "Die neue Quelle kam nicht in die Konfiguration.";
    EXPECT_TRUE(listShows(ITEM_NAME.ToStdWstring()));
}

TEST_F(GuiTest, SynchronisationBearbeitenUeberDoppelklickInListe)  // [SyncItemManagementTest.md#edit-routes]
{
    const SyncItemInput input = sandboxSync();
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    ListCtrl* list = syncItemList();
    ASSERT_NE(list, nullptr);

    const SyncItemInput edited = withNewSource(input);
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), fillSyncItemDialog(edited));
    ASSERT_TRUE(clickOn(*list, listRowOf(*list, ITEM_NAME), true)) << "Der Doppelklick ließ sich nicht ausführen.";
    pump();

    EXPECT_EQ(sourceOf(ITEM_NAME), edited.source.ToStdWstring()) << "Der Doppelklick öffnete keinen Dialog oder die Änderung kam nicht an.";
}

TEST_F(GuiTest, SynchronisationBearbeitenUeberZeilenkopfInTabelle)  // [SyncItemManagementTest.md#edit-routes]
{
    const SyncItemInput input = sandboxSync();
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    showView(mainWindow(), id::View_Grid);
    GridCtrl* grid = gridView(mainWindow());
    ASSERT_NE(grid, nullptr) << "Die Tabellenansicht wurde nicht angezeigt.";
    ASSERT_EQ(grid->GetNumberRows(), 1);
    ASSERT_EQ(grid->GetRowLabelValue(0), ITEM_NAME);

    const SyncItemInput edited = withNewSource(input);
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), fillSyncItemDialog(edited));
    ASSERT_TRUE(clickOnRowLabel(*grid, 0)) << "Der Zeilenkopf ließ sich nicht anklicken.";
    pump();

    EXPECT_EQ(sourceOf(ITEM_NAME), edited.source.ToStdWstring()) << "Der Klick auf den Zeilenkopf öffnete keinen Dialog oder die Änderung kam nicht an.";
    EXPECT_EQ(grid->GetCellValue(0, 1), edited.source) << "Die Tabelle zeigt die neue Quelle nicht.";
}

TEST_F(GuiTest, SynchronisationLoeschenFragtNachUndEntfernt)  // [SyncItemManagementTest.md#delete]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));
    ListCtrl* list = syncItemList();
    ASSERT_NE(list, nullptr);
    ASSERT_TRUE(clickOn(*list, listRowOf(*list, ITEM_NAME)));

    ModalDialogWatcher::instance().answer(confirm_delete(), wxID_YES);
    activateMenuItem(mainWindow(), tul(id::DeleteSync));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Es wurde ohne Rückfrage gelöscht.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().front().Contains(ITEM_NAME)) << "Die Rückfrage nennt die Synchronisation nicht.";
    EXPECT_FALSE(Config::instance().find_by_name(ITEM_NAME.ToStdWstring()).has_value()) << "Die Synchronisation steht noch in der Konfiguration.";
    EXPECT_FALSE(listShows(ITEM_NAME.ToStdWstring())) << "Die Synchronisation steht noch in der Liste.";
}

TEST_F(GuiTest, SynchronisationLoeschenAbbrechenBehaelt)  // [SyncItemManagementTest.md#delete]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));
    ListCtrl* list = syncItemList();
    ASSERT_NE(list, nullptr);
    ASSERT_TRUE(clickOn(*list, listRowOf(*list, ITEM_NAME)));

    ModalDialogWatcher::instance().answer(confirm_delete(), wxID_NO);
    activateMenuItem(mainWindow(), tul(id::DeleteSync));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u);
    EXPECT_TRUE(Config::instance().find_by_name(ITEM_NAME.ToStdWstring()).has_value()) << "Trotz Nein wurde gelöscht.";
    EXPECT_TRUE(listShows(ITEM_NAME.ToStdWstring()));
}

TEST_F(GuiTest, LeererNameWirdAbgelehnt)  // [SyncItemManagementTest.md#rejected-names]
{
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fillSyncItemDialog(sandboxSync("")));
    ModalDialogWatcher::instance().answer(translation::translate("error"), wxID_OK);

    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der leere Name wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("name_empty"));
    EXPECT_TRUE(Config::instance().syncitems().empty()) << "Eine Synchronisation ohne Namen wurde angelegt.";
}

TEST_F(GuiTest, DoppelterNameWirdAbgelehnt)  // [SyncItemManagementTest.md#rejected-names]
{
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));

    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fillSyncItemDialog(sandboxSync()));
    ModalDialogWatcher::instance().answer(translation::translate("error"), wxID_OK);
    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der doppelte Name wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("sync_exists", {ITEM_NAME.ToStdWstring()}));
    EXPECT_EQ(Config::instance().syncitems().size(), 1u) << "Die Synchronisation wurde doppelt angelegt.";
}

}
