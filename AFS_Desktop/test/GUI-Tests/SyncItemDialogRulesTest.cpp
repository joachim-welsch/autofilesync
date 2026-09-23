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
#include "UserActions.h"
#include "Config.h"
#include "ListCtrl.h"
#include "SyncItemDialog.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <wx/spinctrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

using DialogCheck = std::function<void(wxDialog&)>;

DialogAction inspectAndCancel(DialogCheck check)  // [SyncItemDialogRulesTest.md#inspectAndCancel]
{
    return [check](wxDialog& dialog) {
        if (waitUntil([&dialog] { return dialog.IsShownOnScreen(); }))
            check(dialog);
        else
            ADD_FAILURE() << "Der Dialog erschien nicht.";
        dialog.EndModal(wxID_CANCEL);
    };
}

bool okEnabled(wxDialog& dialog)  // [SyncItemDialogRulesTest.md#okEnabled]
{
    wxWindow* ok = find(dialog, wxID_OK);
    pump();
    return ok != nullptr && ok->IsEnabled();
}

void openNewSyncItemDialog(MainWindow& window, DialogCheck check)
{
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, inspectAndCancel(std::move(check)));
    activateMenuItem(window, tul(id::File_new_Synchronization));
    pump();
}

}

TEST_F(GuiTest, OkNurMitQuelleZielTypUndVergleichskriterium)
{
    openNewSyncItemDialog(mainWindow(), [](wxDialog& dialog) {
        EXPECT_FALSE(okEnabled(dialog)) << "OK ist ohne Quelle und Ziel aktiv.";
        ASSERT_TRUE(replaceText(find(dialog, ID_SOURCE_DIR_BOX), GuiTestSandbox::sourceDir().string()));
        EXPECT_FALSE(okEnabled(dialog)) << "OK ist ohne Ziel aktiv.";
        ASSERT_TRUE(replaceText(find(dialog, ID_TARGET_DIR_BOX), GuiTestSandbox::targetDir().string()));
        EXPECT_TRUE(okEnabled(dialog)) << "OK bleibt trotz Quelle, Ziel, Typ und Datumsvergleich gesperrt.";

        ASSERT_TRUE(showNotebookPage(dialog, _("Optionen")));
        ASSERT_TRUE(setCheckBox(find(dialog, ID_COMPARE_DATE_CHECK), false));
        EXPECT_FALSE(okEnabled(dialog)) << "OK ist ohne Vergleichskriterium aktiv.";
        ASSERT_TRUE(setCheckBox(find(dialog, ID_COMPARE_SIZE_CHECK), true));
        EXPECT_TRUE(okEnabled(dialog)) << "OK bleibt mit Größenvergleich gesperrt.";
    });
    EXPECT_TRUE(Config::instance().syncitems().empty());
}

TEST_F(GuiTest, ZweiWegeSyncSperrtKopierenWennZielNeuer)  // [SyncItemDialogRulesTest.md#option-locks]
{
    openNewSyncItemDialog(mainWindow(), [](wxDialog& dialog) {
        ASSERT_TRUE(showNotebookPage(dialog, _("Optionen")));
        wxWindow* copyWhenNewer = find(dialog, ID_CPY_WHEN_TARGET_NEWER_CHECK);
        ASSERT_TRUE(setCheckBox(copyWhenNewer, true));

        ASSERT_TRUE(setCheckBox(find(dialog, ID_TWO_WAY_SYNC_CHECK), true));
        EXPECT_FALSE(copyWhenNewer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" bleibt bei Zwei-Wege-Sync bedienbar.";
        EXPECT_FALSE(static_cast<wxCheckBox*>(copyWhenNewer)->GetValue()) << "\"Kopieren wenn Ziel neuer\" bleibt bei Zwei-Wege-Sync gesetzt.";

        ASSERT_TRUE(setCheckBox(find(dialog, ID_TWO_WAY_SYNC_CHECK), false));
        EXPECT_TRUE(copyWhenNewer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" wird nach dem Abwählen nicht wieder frei.";
    });
}

TEST_F(GuiTest, TypDateiSperrtLoeschenImZiel)  // [SyncItemDialogRulesTest.md#option-locks]
{
    openNewSyncItemDialog(mainWindow(), [](wxDialog& dialog) {
        wxWindow* deleteInTarget = find(dialog, ID_DELETE_IN_TARGET_CHECK);
        ASSERT_TRUE(showNotebookPage(dialog, _("Optionen")));
        EXPECT_FALSE(deleteInTarget->IsEnabled()) << "\"Gelöschte Dateien im Ziel löschen\" ist beim Typ Datei bedienbar.";

        ASSERT_TRUE(showNotebookPage(dialog, _("Allgemein")));
        ASSERT_TRUE(selectEntry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Dir)));
        ASSERT_TRUE(showNotebookPage(dialog, _("Optionen")));
        EXPECT_TRUE(deleteInTarget->IsEnabled()) << "\"Gelöschte Dateien im Ziel löschen\" bleibt beim Typ Verzeichnis gesperrt.";
        ASSERT_TRUE(setCheckBox(deleteInTarget, true));

        ASSERT_TRUE(showNotebookPage(dialog, _("Allgemein")));
        ASSERT_TRUE(selectEntry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::File)));
        EXPECT_FALSE(static_cast<wxCheckBox*>(deleteInTarget)->GetValue()) << "Der Wechsel zurück zu Datei nimmt die Option nicht zurück.";
    });
}

TEST_F(GuiTest, ZyklusIstAuf1Bis60Begrenzt)
{
    openNewSyncItemDialog(mainWindow(), [](wxDialog& dialog) {
        ASSERT_TRUE(showNotebookPage(dialog, _("Optionen")));
        auto* spin = dynamic_cast<wxSpinCtrl*>(find(dialog, ID_SYNC_CYCLE_SPIN));
        ASSERT_NE(spin, nullptr);
        EXPECT_EQ(spin->GetMin(), 1);
        EXPECT_EQ(spin->GetMax(), 60);
        EXPECT_EQ(spin->GetValue(), 1) << "Die Vorgabe ist nicht 1 s.";
        EXPECT_FALSE(setSpinValue(spin, 0)) << "0 Sekunden wurden angenommen.";
        EXPECT_FALSE(setSpinValue(spin, 61)) << "61 Sekunden wurden angenommen.";
        EXPECT_TRUE(setSpinValue(spin, 60));
    });
}

TEST_F(GuiTest, BearbeitenZeigtDieGespeichertenAusschluesseZeilenweise)  // [SyncItemDialogRulesTest.md#exclusions-round-trip]
{
    SyncItem item(L"guitest-ausschluesse", SyncItemType::Dir, GuiTestSandbox::sourceDir().wstring(), GuiTestSandbox::targetDir().wstring(), L"*.*");
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    exclusions.push_back(L"geheim*");
    item.exclusions(exclusions);
    Config::instance().add(item);
    if (ListCtrl* list = syncItemList())
        list->update();
    ModalDialogWatcher::instance().expect(txt::EditSynchronisation, inspectAndCancel([](wxDialog& dialog) {
        ASSERT_TRUE(showNotebookPage(dialog, _(L"Ausschlüsse")));
        EXPECT_EQ(valueOf(find(dialog, ID_EXCLUSIONS_BOX)), "*.log\ngeheim*");
    }));
    ASSERT_TRUE(syncItemList() != nullptr && clickOn(*syncItemList(), 0));

    activateMenuItem(mainWindow(), tul(id::EditSync));
    pump();
}

}
