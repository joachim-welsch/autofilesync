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
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "PreviewDialog.h"
#include "SyncProposal.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/listctrl.h>
#include <vector>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

struct ShownStep { wxString action, item, file; };

std::vector<ShownStep> shown_steps(wxDialog& dialog)
{
    std::vector<ShownStep> steps;
    wxListCtrl& list = dynamic_cast<PreviewDialog&>(dialog).list();
    for (long row = 0; row < list.GetItemCount(); ++row)
        steps.push_back({list.GetItemText(row, 0), list.GetItemText(row, 1), list.GetItemText(row, 2)});
    return steps;
}

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

std::filesystem::path unmounted_dir(const SandboxTree& tree)  // [PreviewTest.md#unmounted_dir]
{
#ifdef _WIN32
    std::error_code ec;
    for (wchar_t letter = L'Z'; letter > L'C'; --letter)
        if (const std::filesystem::path drive = std::wstring{letter, L':', L'\\'}; !std::filesystem::exists(drive, ec))
            return drive / "nicht-angeschlossen";
#endif
    return tree.path("nicht-angeschlossen");
}

bool preview(MainWindow& window)  // [PreviewTest.md#preview]
{
    activate_menu_item(window, tul(id::Preview));
    pump();
    return wait_until_sync_finished(window);
}

const ShownStep* step_with(const std::vector<ShownStep>& steps, const wxString& action)  // [PreviewTest.md#step_with]
{
    for (const ShownStep& step : steps)
        if (step.action == action)
            return &step;
    return nullptr;
}

}

TEST_F(GuiTest, VorschauZeigtKopienUndLoeschungenUndAendertNichts)  // [PreviewTest.md#VorschauZeigtKopienUndLoeschungenUndAendertNichts]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau", {{"quelle/neu.txt", "neu"}, {"ziel/alt.txt", "alt"}});
    SyncItemInput input = dir_sync(tree, "guitest-vorschau");
    input.options.delete_in_target = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    const std::vector<std::string> before = tree.entries();

    std::vector<ShownStep> steps;
    bool run_button_enabled = false;
    ModalDialogWatcher::instance().expect(translation::translate("preview_title"), [&](wxDialog& dialog) {
        steps = shown_steps(dialog);
        run_button_enabled = find(dialog, wxID_OK)->IsEnabled();
        click_on(find(dialog, wxID_CANCEL));
    });
    ASSERT_TRUE(preview(main_window())) << "Die Vorschau kam nicht zur Ruhe.";

    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Ein unerwarteter Dialog erschien.";
    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Der Vorschau-Dialog erschien nicht.";
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].action, translation::translate("preview_copy_to_target"));
    EXPECT_EQ(steps[0].item, "guitest-vorschau");
    EXPECT_EQ(steps[0].file, "neu.txt") << "Die Datei wird nicht relativ zur Quelle genannt.";
    EXPECT_EQ(steps[1].action, translation::translate("preview_delete_in_target"));
    EXPECT_EQ(steps[1].file, "alt.txt") << "Die Datei wird nicht relativ zum Ziel genannt.";
    EXPECT_TRUE(run_button_enabled);
    EXPECT_EQ(tree.entries(), before) << "Die Vorschau hat das Dateisystem verändert.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::Preview))) << "\"Vorschau\" bleibt nach dem Schließen gesperrt.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce)));
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
}

TEST_F(GuiTest, JetztSynchronisierenInDerVorschauFuehrtDenLaufAus)  // [PreviewTest.md#JetztSynchronisierenInDerVorschauFuehrtDenLaufAus]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau-lauf", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-vorschau-lauf")));

    ModalDialogWatcher::instance().expect(translation::translate("preview_title"), [](wxDialog& dialog) {
        click_on(find(dialog, wxID_OK));
    });
    ASSERT_TRUE(preview(main_window())) << "Der Lauf nach der Vorschau kam nicht zur Ruhe.";

    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Der Vorschau-Dialog erschien nicht.";
    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht") << "\"Jetzt synchronisieren\" hat nicht kopiert.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::Preview)));
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce)));
}

TEST_F(GuiTest, VorgeschlageneVorschauZeigtDenDialog)  // [PreviewTest.md#VorgeschlageneVorschauZeigtDenDialog]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau-befehl", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-vorschau-befehl")));
    std::vector<ShownStep> steps;
    ModalDialogWatcher::instance().expect(translation::translate("preview_title"), [&steps](wxDialog& dialog) {
        steps = shown_steps(dialog);
        click_on(find(dialog, wxID_CANCEL));
    });

    main_window().apply_proposal(sync_proposal::Command::Preview);
    pump();

    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Die vorgeschlagene Vorschau kam nicht zur Ruhe.";
    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Der Vorschau-Dialog erschien nicht.";
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0].file, "bericht.txt");
    EXPECT_FALSE(tree.exists("ziel/bericht.txt")) << "Der vorgeschlagene Befehl hat kopiert statt nur gezeigt.";
}

TEST_F(GuiTest, VorschauMitNichtErreichbaremZielWarntStattNichtsZuTun)  // [PreviewTest.md#VorschauMitNichtErreichbaremZielWarntStattNichtsZuTun]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau-weg", {{"quelle/bericht.txt", "Bericht"}});
    SyncItemInput input{"guitest-vorschau-weg", tree.path("quelle").string(), unmounted_dir(tree).string()};
    input.type = SyncItemType::Dir;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ModalDialogWatcher::instance().answer(translation::translate("preview_title"), wxID_OK);
    ASSERT_TRUE(preview(main_window()));

    const std::vector<wxString>& answered = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(answered.size(), 1u) << "Die Vorschau zeigte nicht genau eine Meldung.";
    const wxString expected = translation::translate("preview_unreachable", {L"\nguitest-vorschau-weg"});
    EXPECT_TRUE(answered.front().StartsWith(expected)) << answered.front();
    EXPECT_FALSE(answered.front().Contains(translation::translate("preview_nothing_to_do"))) << "Ein fehlendes Ziel darf nicht als Übereinstimmung gelten.";
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Ein unerwarteter Dialog erschien.";
}

TEST_F(GuiTest, VorschauOhneUnterschiedMeldetNichtsZuTun)  // [PreviewTest.md#VorschauOhneUnterschiedMeldetNichtsZuTun]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau-leer", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-vorschau-leer")));
    ASSERT_TRUE(sync_once(main_window()));

    ModalDialogWatcher::instance().answer(translation::translate("preview_title"), wxID_OK);
    ASSERT_TRUE(preview(main_window()));

    const std::vector<wxString>& answered = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(answered.size(), 1u) << "Die Vorschau zeigte nicht genau eine Meldung.";
    EXPECT_EQ(answered.front(), translation::translate("preview_nothing_to_do"));
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Trotz leerer Vorschau erschien ein Dialog.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::Preview)));
}


TEST_F(GuiTest, VorschauZeigtUmbenennungUndLoeschungInDerQuelleBeiZweiWege)  // [PreviewTest.md#VorschauZeigtUmbenennungUndLoeschungInDerQuelleBeiZweiWege]
{
    const SandboxTree tree(GuiTestSandbox::root() / "vorschau-zweiwege", {{"quelle/alt.txt", "umbenannt"}, {"quelle/weg.txt", "im Ziel gelöscht"}});
    std::filesystem::create_directories(tree.path("ziel"));  // [PreviewTest.md#existing-target]
    SyncItemInput input = dir_sync(tree, "guitest-vorschau-zweiwege");
    input.options.two_way_sync = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    std::filesystem::rename(tree.path("quelle/alt.txt"), tree.path("quelle/neu.txt"));
    std::filesystem::remove(tree.path("ziel/weg.txt"));
    const std::vector<std::string> before = tree.entries();

    std::vector<ShownStep> steps;
    ModalDialogWatcher::instance().expect(translation::translate("preview_title"), [&](wxDialog& dialog) {
        steps = shown_steps(dialog);
        click_on(find(dialog, wxID_CANCEL));
    });
    ASSERT_TRUE(preview(main_window())) << "Die Vorschau kam nicht zur Ruhe.";

    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Der Vorschau-Dialog erschien nicht.";
    ASSERT_EQ(steps.size(), 2u);
    const ShownStep* renamed = step_with(steps, translation::translate("preview_rename_in_target"));
    ASSERT_NE(renamed, nullptr) << "Die Umbenennung wurde nicht erkannt.";
    EXPECT_EQ(renamed->file, wxString::FromUTF8("alt.txt \xe2\x86\x92 neu.txt")) << "Alter und neuer Name fehlen.";
    const ShownStep* deleted = step_with(steps, translation::translate("preview_delete_in_source"));
    ASSERT_NE(deleted, nullptr) << "Die Löschung im Ziel wurde nicht als Löschung in der Quelle geplant.";
    EXPECT_EQ(deleted->file, "weg.txt");
    EXPECT_EQ(tree.entries(), before) << "Die Vorschau hat das Dateisystem verändert.";
}

}
