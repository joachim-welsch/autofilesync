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
#include "UserActions.h"
#include "Config.h"
#include "ListCtrl.h"
#include "SyncItemDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/spinctrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

using DialogCheck = std::function<void(wxDialog&)>;

DialogAction inspect_and_cancel(DialogCheck check)  // [SyncItemDialogRulesTest.md#inspect_and_cancel]
{
    return [check](wxDialog& dialog) {
        if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }))
            check(dialog);
        else
            ADD_FAILURE() << "Der Dialog erschien nicht.";
        dialog.EndModal(wxID_CANCEL);
    };
}

bool ok_enabled(wxDialog& dialog)  // [SyncItemDialogRulesTest.md#ok_enabled]
{
    wxWindow* ok = find(dialog, wxID_OK);
    pump();
    return ok != nullptr && ok->IsEnabled();
}

void open_new_sync_item_dialog(MainWindow& window, DialogCheck check)
{
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), inspect_and_cancel(std::move(check)));
    activate_menu_item(window, tul(id::File_new_Synchronization));
    pump();
}

}

TEST_F(GuiTest, OkNurMitQuelleZielTypUndVergleichskriterium)
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        EXPECT_FALSE(ok_enabled(dialog)) << "OK ist ohne Quelle und Ziel aktiv.";
        ASSERT_TRUE(replace_text(find(dialog, ID_SOURCE_DIR_BOX), GuiTestSandbox::source_dir().string()));
        EXPECT_FALSE(ok_enabled(dialog)) << "OK ist ohne Ziel aktiv.";
        ASSERT_TRUE(replace_text(find(dialog, ID_TARGET_DIR_BOX), GuiTestSandbox::target_dir().string()));
        EXPECT_TRUE(ok_enabled(dialog)) << "OK bleibt trotz Quelle, Ziel, Typ und Datumsvergleich gesperrt.";

        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        ASSERT_TRUE(set_check_box(find(dialog, ID_COMPARE_DATE_CHECK), false));
        EXPECT_FALSE(ok_enabled(dialog)) << "OK ist ohne Vergleichskriterium aktiv.";
        ASSERT_TRUE(set_check_box(find(dialog, ID_COMPARE_SIZE_CHECK), true));
        EXPECT_TRUE(ok_enabled(dialog)) << "OK bleibt mit Größenvergleich gesperrt.";
    });
    EXPECT_TRUE(Config::instance().syncitems().empty());
}

TEST_F(GuiTest, ZweiWegeSyncSperrtKopierenWennZielNeuer)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* copy_when_newer = find(dialog, ID_CPY_WHEN_TARGET_NEWER_CHECK);  // not set beforehand: that would lock two-way sync itself

        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), true));
        EXPECT_FALSE(copy_when_newer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" bleibt bei Zwei-Wege-Sync bedienbar.";
        EXPECT_FALSE(static_cast<wxCheckBox*>(copy_when_newer)->GetValue()) << "\"Kopieren wenn Ziel neuer\" bleibt bei Zwei-Wege-Sync gesetzt.";

        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), false));
        EXPECT_TRUE(copy_when_newer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" wird nach dem Abwählen nicht wieder frei.";
    });
}

TEST_F(GuiTest, VerschiebenSperrtZweiWegeUndLoeschenImZiel)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_general")));
        ASSERT_TRUE(select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Dir)));
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* move = find(dialog, ID_DEL_SOURCE_AFTER_CHECK);
        wxWindow* two_way = find(dialog, ID_TWO_WAY_SYNC_CHECK);
        wxWindow* delete_in_target = find(dialog, ID_DELETE_IN_TARGET_CHECK);

        ASSERT_TRUE(set_check_box(delete_in_target, true));
        EXPECT_FALSE(move->IsEnabled()) << "\"Quelle nach dem Kopieren löschen\" bleibt bei Löschen im Ziel bedienbar.";
        ASSERT_TRUE(set_check_box(delete_in_target, false));
        EXPECT_TRUE(move->IsEnabled()) << "\"Quelle nach dem Kopieren löschen\" wird nach dem Abwählen nicht wieder frei.";

        ASSERT_TRUE(set_check_box(move, true));
        EXPECT_FALSE(two_way->IsEnabled()) << "Zwei-Wege-Sync bleibt beim Verschieben bedienbar.";
        EXPECT_FALSE(delete_in_target->IsEnabled()) << "Löschen im Ziel bleibt beim Verschieben bedienbar.";
        ASSERT_TRUE(set_check_box(move, false));
        EXPECT_TRUE(two_way->IsEnabled()) << "Zwei-Wege-Sync wird nach dem Abwählen nicht wieder frei.";
        EXPECT_TRUE(delete_in_target->IsEnabled()) << "Löschen im Ziel wird nach dem Abwählen nicht wieder frei.";
    });
}

TEST_F(GuiTest, ZweiWegeSyncSperrtVerschieben)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* move = find(dialog, ID_DEL_SOURCE_AFTER_CHECK);

        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), true));
        EXPECT_FALSE(move->IsEnabled()) << "\"Quelle nach dem Kopieren löschen\" bleibt bei Zwei-Wege-Sync bedienbar.";
        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), false));
        EXPECT_TRUE(move->IsEnabled()) << "\"Quelle nach dem Kopieren löschen\" wird nach dem Abwählen nicht wieder frei.";
    });
}

TEST_F(GuiTest, KopierenWennZielNeuerSperrtZweiWegeSync)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* two_way = find(dialog, ID_TWO_WAY_SYNC_CHECK);

        ASSERT_TRUE(set_check_box(find(dialog, ID_CPY_WHEN_TARGET_NEWER_CHECK), true));
        EXPECT_FALSE(two_way->IsEnabled()) << "Zwei-Wege-Sync bleibt bei \"Kopieren wenn Ziel neuer\" bedienbar.";
        ASSERT_TRUE(set_check_box(find(dialog, ID_CPY_WHEN_TARGET_NEWER_CHECK), false));
        EXPECT_TRUE(two_way->IsEnabled()) << "Zwei-Wege-Sync wird nach dem Abwählen nicht wieder frei.";
    });
}

TEST_F(GuiTest, KopierenWennZielNeuerBrauchtZeitOderGroesse)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* copy_when_newer = find(dialog, ID_CPY_WHEN_TARGET_NEWER_CHECK);
        ASSERT_TRUE(set_check_box(copy_when_newer, true));

        ASSERT_TRUE(set_check_box(find(dialog, ID_COMPARE_DATE_CHECK), false));
        EXPECT_FALSE(copy_when_newer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" bleibt ohne Zeit- und Größenvergleich bedienbar.";
        EXPECT_FALSE(static_cast<wxCheckBox*>(copy_when_newer)->GetValue()) << "\"Kopieren wenn Ziel neuer\" bleibt ohne Zeit- und Größenvergleich gesetzt.";

        ASSERT_TRUE(set_check_box(find(dialog, ID_COMPARE_SIZE_CHECK), true));
        EXPECT_TRUE(copy_when_newer->IsEnabled()) << "\"Kopieren wenn Ziel neuer\" wird mit dem Größenvergleich nicht wieder frei.";
    });
}

TEST_F(GuiTest, TypDateiSperrtLoeschenImZiel)  // [SyncItemDialogRulesTest.md#option-locks]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        wxWindow* delete_in_target = find(dialog, ID_DELETE_IN_TARGET_CHECK);
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        EXPECT_FALSE(delete_in_target->IsEnabled()) << "\"Gelöschte Dateien im Ziel löschen\" ist beim Typ Datei bedienbar.";

        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_general")));
        ASSERT_TRUE(select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Dir)));
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        EXPECT_TRUE(delete_in_target->IsEnabled()) << "\"Gelöschte Dateien im Ziel löschen\" bleibt beim Typ Verzeichnis gesperrt.";
        ASSERT_TRUE(set_check_box(delete_in_target, true));

        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_general")));
        ASSERT_TRUE(select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::File)));
        EXPECT_FALSE(static_cast<wxCheckBox*>(delete_in_target)->GetValue()) << "Der Wechsel zurück zu Datei nimmt die Option nicht zurück.";
    });
}

TEST_F(GuiTest, ZyklusIstAuf1Bis60Begrenzt)
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        auto* spin = dynamic_cast<wxSpinCtrl*>(find(dialog, ID_SYNC_CYCLE_SPIN));
        ASSERT_NE(spin, nullptr);
        EXPECT_EQ(spin->GetMin(), 1);
        EXPECT_EQ(spin->GetMax(), 60);
        EXPECT_EQ(spin->GetValue(), 1) << "Die Vorgabe ist nicht 1 s.";
        EXPECT_FALSE(set_spin_value(spin, 0)) << "0 Sekunden wurden angenommen.";
        EXPECT_FALSE(set_spin_value(spin, 61)) << "61 Sekunden wurden angenommen.";
        EXPECT_TRUE(set_spin_value(spin, 60));
    });
}

TEST_F(GuiTest, BearbeitenZeigtDieGespeichertenAusschluesseZeilenweise)  // [SyncItemDialogRulesTest.md#exclusions-round-trip]
{
    SyncItem item(L"guitest-ausschluesse", SyncItemType::Dir, GuiTestSandbox::source_dir().wstring(), GuiTestSandbox::target_dir().wstring(), L"*.*");
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    exclusions.push_back(L"geheim*");
    item.exclusions(exclusions);
    Config::instance().add(item);
    if (ListCtrl* list = sync_item_list())
        list->update();
    ModalDialogWatcher::instance().expect(txt::edit_synchronisation(), inspect_and_cancel([](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_exclusions")));
        EXPECT_EQ(value_of(find(dialog, ID_EXCLUSIONS_BOX)), "*.log\ngeheim*");
    }));
    ASSERT_TRUE(sync_item_list() != nullptr && click_on(*sync_item_list(), 0));

    activate_menu_item(main_window(), tul(id::EditSync));
    pump();
}


TEST_F(GuiTest, ZweiWegeSyncSetztUmbenennenErkennenUndZeigtDenHinweis)  // [SyncItemDialogRulesTest.md#two-way-implies-moves]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_general")));
        ASSERT_TRUE(select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Dir)));
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        wxWindow* track_moves = find(dialog, ID_TRACK_MOVES_CHECK);
        wxWindow* hint = find(dialog, ID_TWO_WAY_HINT);
        ASSERT_NE(track_moves, nullptr);
        ASSERT_NE(hint, nullptr);
        EXPECT_TRUE(track_moves->IsEnabled()) << "\"Umbenennen und Verschieben erkennen\" ist ohne Zwei-Wege-Sync gesperrt.";
        EXPECT_FALSE(hint->IsShown()) << "Der Zwei-Wege-Hinweis erscheint ohne Zwei-Wege-Sync.";

        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), true));
        EXPECT_TRUE(static_cast<wxCheckBox*>(track_moves)->GetValue()) << "Zwei-Wege-Sync setzt \"Umbenennen und Verschieben erkennen\" nicht.";
        EXPECT_FALSE(track_moves->IsEnabled()) << "\"Umbenennen und Verschieben erkennen\" bleibt bei Zwei-Wege-Sync bedienbar.";
        EXPECT_TRUE(hint->IsShown()) << "Der Zwei-Wege-Hinweis fehlt.";

        ASSERT_TRUE(set_check_box(find(dialog, ID_TWO_WAY_SYNC_CHECK), false));
        EXPECT_TRUE(track_moves->IsEnabled()) << "\"Umbenennen und Verschieben erkennen\" wird nach dem Abwählen nicht wieder frei.";
        EXPECT_FALSE(hint->IsShown()) << "Der Zwei-Wege-Hinweis bleibt nach dem Abwählen stehen.";
    });
}

TEST_F(GuiTest, TypDateiSperrtUmbenennenErkennen)  // [SyncItemDialogRulesTest.md#two-way-implies-moves]
{
    open_new_sync_item_dialog(main_window(), [](wxDialog& dialog) {
        wxWindow* track_moves = find(dialog, ID_TRACK_MOVES_CHECK);
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        EXPECT_FALSE(track_moves->IsEnabled()) << "\"Umbenennen und Verschieben erkennen\" ist beim Typ Datei bedienbar.";

        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_general")));
        ASSERT_TRUE(select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Tree)));
        ASSERT_TRUE(show_notebook_page(dialog, translation::translate("tab_options")));
        EXPECT_TRUE(track_moves->IsEnabled()) << "\"Umbenennen und Verschieben erkennen\" bleibt beim Typ Verzeichnisbaum gesperrt.";
    });
}

}
