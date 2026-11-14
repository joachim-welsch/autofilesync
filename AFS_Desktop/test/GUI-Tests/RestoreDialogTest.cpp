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
#include "RestoredSettings.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "UserActions.h"
#include "Config.h"
#include "FileDelete.h"
#include "RecycleBin.h"
#include "RestoreDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <algorithm>
#include <filesystem>
#include <functional>

using namespace AFS_Core;
namespace fs = std::filesystem;

using utilwx::tul;

namespace guitest {
namespace {

constexpr int SEARCH_TIMEOUT_MILLISECONDS = 5000;

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

wxListCtrl& list_of(wxDialog& dialog)
{
    return *static_cast<wxListCtrl*>(find(dialog, ID_RESTORE_LIST));
}

wxString status_of(wxDialog& dialog)  // [RestoreDialogTest.md#status_of]
{
    wxString text = static_cast<wxStaticText*>(find(dialog, ID_RESTORE_STATUS))->GetLabel();
    text.Replace("\n", " ");
    return text;
}

bool shows_rows(wxDialog& dialog, long rows)
{
    return wait_until([&dialog, rows] { return list_of(dialog).GetItemCount() == rows; }, SEARCH_TIMEOUT_MILLISECONDS);
}

bool search_finished(wxDialog& dialog)
{
    return wait_until([&dialog] { return status_of(dialog) != wxString(translation::translate("restore_searching")); }, SEARCH_TIMEOUT_MILLISECONDS);
}

bool restore_row(wxDialog& dialog, long row, int button = ID_RESTORE_PUT_BACK)
{
    const bool clicked = click_on(list_of(dialog), row) && click_when_enabled(find(dialog, button));
    pump();
    return clicked;
}

class RestoreDialogTest: public GuiTest {  // [RestoreDialogTest.md#RestoreDialogTest]
protected:
    RestoredSettings restored_settings_;
    fs::path bin_ = GuiTestSandbox::root() / "papierkorb-wiederherstellen";

    void SetUp() override
    {
        GuiTest::SetUp();
        fs::remove_all(bin_);
        RecycleBin::instance().bin_path(bin_);
        Config::instance().settings().delete_mode(DeleteMode::RecycleBin);
    }

    void TearDown() override
    {
        fs::remove_all(bin_);
        GuiTest::TearDown();
    }

    void in_restore_dialog(const std::function<void(wxDialog&)>& use)  // [RestoreDialogTest.md#in_restore_dialog]
    {
        ModalDialogWatcher::instance().expect(txt::restore_files(), [use](wxDialog& dialog) {
            if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }) && search_finished(dialog))
                use(dialog);
            else
                ADD_FAILURE() << "Der Dialog erschien nicht oder die Suche endete nicht.";
            dialog.EndModal(wxID_CANCEL);
        });
        activate_menu_item(main_window(), tul(id::RestoreFiles));
        pump();
    }
};

}

TEST_F(RestoreDialogTest, OhneFundErklaertDerDialogWoherDateienKommen)
{
    in_restore_dialog([](wxDialog& dialog) {
        EXPECT_EQ(list_of(dialog).GetItemCount(), 0);
        EXPECT_EQ(status_of(dialog), wxString(translation::translate("restore_nothing_found")));
        pump();
        EXPECT_FALSE(find(dialog, ID_RESTORE_PUT_BACK)->IsEnabled()) << "Wiederherstellen ist ohne Auswahl bedienbar.";
        EXPECT_FALSE(find(dialog, ID_RESTORE_PUT_ELSEWHERE)->IsEnabled());
    });
}

TEST_F(RestoreDialogTest, GeloeschteDateiKommtAnIhrenPlatzZurueck)
{
    const SandboxTree tree(GuiTestSandbox::root() / "wiederherstellen", {{"briefe/anna.txt", "liebe anna"}});
    ASSERT_TRUE(delete_file(tree.path("briefe/anna.txt").wstring()));
    in_restore_dialog([&tree](wxDialog& dialog) {
        ASSERT_TRUE(shows_rows(dialog, 1));
        wxListCtrl& list = list_of(dialog);
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_FILE), "anna.txt");
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_FOLDER), wxString(tree.path("briefe").wstring()));
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_KIND), wxString(translation::translate("restore_kind_deleted")));

        ASSERT_TRUE(restore_row(dialog, 0));
        EXPECT_EQ(tree.read("briefe/anna.txt"), "liebe anna");
        EXPECT_EQ(list.GetItemCount(), 0) << "Die wiederhergestellte Datei steht noch in der Liste.";
        EXPECT_EQ(status_of(dialog), wxString(translation::translate("restore_result_one", {L"anna.txt", tree.path("briefe").wstring()})));
    });
}

TEST_F(RestoreDialogTest, VorhandeneDateiWirdNichtUeberschrieben)
{
    const SandboxTree tree(GuiTestSandbox::root() / "wiederherstellen", {{"bericht.txt", "alt"}});
    ASSERT_TRUE(delete_file(tree.path("bericht.txt").wstring()));
    tree.write("bericht.txt", "neu");
    in_restore_dialog([&tree](wxDialog& dialog) {
        ASSERT_TRUE(shows_rows(dialog, 1));
        ASSERT_TRUE(restore_row(dialog, 0));
        EXPECT_EQ(tree.read("bericht.txt"), "neu");
        const std::vector<std::string> entries = tree.entries();
        EXPECT_EQ(std::ranges::count_if(entries, [](const std::string& entry) { return entry.starts_with("bericht ("); }), 1)
            << "Die gelöschte Fassung liegt nicht neben der vorhandenen Datei.";
        EXPECT_TRUE(status_of(dialog).Contains("bericht (")) << status_of(dialog);
    });
}

TEST_F(RestoreDialogTest, WiederherstellenNachLegtDieDateiInDenGewaehltenOrdner)
{
    const SandboxTree tree(GuiTestSandbox::root() / "wiederherstellen", {{"bild.jpg", "jpeg"}, {"gerettet/", ""}});
    ASSERT_TRUE(delete_file(tree.path("bild.jpg").wstring()));
    in_restore_dialog([&tree](wxDialog& dialog) {
        ASSERT_TRUE(shows_rows(dialog, 1));
        ModalDialogWatcher::instance().choose_path(translation::translate("restore_choose_dir_title"), tree.path("gerettet").wstring());
        ASSERT_TRUE(restore_row(dialog, 0, ID_RESTORE_PUT_ELSEWHERE));
        EXPECT_EQ(tree.read("gerettet/bild.jpg"), "jpeg");
        EXPECT_FALSE(tree.exists("bild.jpg"));
    });
}

TEST_F(RestoreDialogTest, SucheGrenztDieListeEin)
{
    const SandboxTree tree(GuiTestSandbox::root() / "wiederherstellen", {{"anna.txt", "a"}, {"berta.txt", "b"}});
    ASSERT_TRUE(delete_file(tree.path("anna.txt").wstring()));
    ASSERT_TRUE(delete_file(tree.path("berta.txt").wstring()));
    in_restore_dialog([](wxDialog& dialog) {
        ASSERT_TRUE(shows_rows(dialog, 2));
        static_cast<wxSearchCtrl*>(find(dialog, ID_RESTORE_FILTER))->SetValue("BERT");
        ASSERT_TRUE(shows_rows(dialog, 1));
        EXPECT_EQ(list_of(dialog).GetItemText(0, RESTORE_COLUMN_FILE), "berta.txt");
    });
}

TEST_F(RestoreDialogTest, FruehereFassungWirdNebenDieAktuelleDateiInDerQuelleKopiert)  // [RestoreDialogTest.md#FruehereFassung]
{
    const SandboxTree tree(GuiTestSandbox::root() / "wiederherstellen",
        {{"quelle/bericht.txt", "aktuell"}, {"ziel/bericht.txt", "aktuell"}, {"ziel/bericht.txt.bak.1", "frueher"}});
    SyncItemInput input = dir_sync(tree, "guitest-wiederherstellen");
    input.options.father_son_principle = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    in_restore_dialog([&tree](wxDialog& dialog) {
        ASSERT_TRUE(shows_rows(dialog, 1));
        wxListCtrl& list = list_of(dialog);
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_FILE), "bericht.txt");
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_FOLDER), wxString(tree.path("quelle").wstring()));
        EXPECT_EQ(list.GetItemText(0, RESTORE_COLUMN_KIND), wxString(translation::translate("restore_kind_version")));

        ASSERT_TRUE(restore_row(dialog, 0));
        EXPECT_EQ(tree.read("quelle/bericht.txt"), "aktuell");
        EXPECT_EQ(tree.read("ziel/bericht.txt.bak.1"), "frueher");
        EXPECT_EQ(list.GetItemCount(), 1) << "Die frühere Fassung muss wählbar bleiben.";
        const std::vector<std::string> entries = tree.entries();
        EXPECT_EQ(std::ranges::count_if(entries, [](const std::string& entry) { return entry.starts_with("quelle/bericht ("); }), 1)
            << "Die frühere Fassung wurde nicht in die Quelle kopiert.";
    });
}

}
