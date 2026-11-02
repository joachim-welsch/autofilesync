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
#include "SshTestServer.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SshAccountDialogDriver.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "SshAccountSelectDialog.h"
#include "SshDirDialog.h"
#include "SyncItemDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/listctrl.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString ACCOUNT = "localhost";  // [SshSyncTest.md#account-named-localhost]
wxString dir_dialog_title()  // translated at the call, once the catalogue is bound
{
    return txt::select_ssh_dir();
}
wxString select_dialog_title()
{
    return txt::select_ssh_connection();
}

bool add_test_account(MainWindow& window)
{
    const SshAccountInput input{ACCOUNT, SSH_TEST_HOST, SSH_TEST_USER, SSH_TEST_PASSWORD};
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fill_ssh_account_dialog(input));
    ModalDialogWatcher::instance().answer(translation::translate("ssh_host_identity"), wxID_YES);  // [SshAccountDialogDriver.md#host-key-question]
    activate_menu_item(window, tul(id::File_new_SshConnection));
    pump();
    return Config::instance().find_ssh_account_by_name(std::string(ACCOUNT.ToUTF8())).has_value();
}

wxListCtrl* dir_list(wxDialog& dialog)
{
    return dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
}

long row_named(wxListCtrl& list, const wxString& text)
{
    for (long row = 0; row < list.GetItemCount(); ++row)
        if (list.GetItemText(row) == text)
            return row;
    return -1;
}

wxString status_of(wxDialog& dialog)  // [SshSyncTest.md#status_of]
{
    const wxString connection_label = wxString(translation::translate("ssh_connection_info")).BeforeFirst('%');
    for (wxWindow* child : dialog.GetChildren())
        if (auto* text = dynamic_cast<wxStaticText*>(child); text != nullptr && text->GetId() == wxID_STATIC && !text->GetLabelText().StartsWith(connection_label))
            return text->GetLabelText();
    return wxString();
}

bool list_shows_file(wxDialog& dialog, const wxString& name)
{
    return wait_until([&dialog, name] { wxListCtrl* list = dir_list(dialog); return list != nullptr && row_named(*list, name) >= 0; });
}

}

TEST_F(SshGuiTest, SshVerzeichnisdialogDoppelklickSteigtAb)
{
    ASSERT_TRUE(sftp().make_dir(probe_dir() + "/unter"));
    ASSERT_TRUE(sftp().create_file(probe_dir() + "/unter/drin.txt"));
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(list_shows_file(dialog, "[unter]")) << "Das Unterverzeichnis erscheint nicht in der Liste.";
        ASSERT_TRUE(click_on(*dir_list(dialog), row_named(*dir_list(dialog), "[unter]"), true));
        EXPECT_TRUE(list_shows_file(dialog, "drin.txt")) << "Der Doppelklick stieg nicht ins Verzeichnis ab.";
        EXPECT_EQ(value_of(find(dialog, ID_SSH_DIR_PATH_BOX)), wxString::FromUTF8(probe_dir() + "/unter"));
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&main_window(), dir_dialog_title(), ssh_test_account("guitest-dialog"), wxString::FromUTF8(probe_dir()));
    dialog.ShowModal();

    sftp().remove_file(probe_dir() + "/unter/drin.txt");
    sftp().remove_dir(probe_dir() + "/unter");
}

TEST_F(SshGuiTest, SshVerzeichnisdialogPfadfeldMitEnterNavigiert)
{
    ASSERT_TRUE(sftp().make_dir(probe_dir() + "/unter"));
    ASSERT_TRUE(sftp().create_file(probe_dir() + "/unter/drin.txt"));
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(list_shows_file(dialog, "[unter]"));
        ASSERT_TRUE(replace_text(find(dialog, ID_SSH_DIR_PATH_BOX), wxString::FromUTF8(probe_dir() + "/unter")));
        ASSERT_TRUE(press_return());
        EXPECT_TRUE(list_shows_file(dialog, "drin.txt")) << "Enter im Pfadfeld wechselte nicht ins Verzeichnis.";
        EXPECT_TRUE(status_of(dialog).Matches(translation::translate("entries_count", {L"*"}))) << status_of(dialog);
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&main_window(), dir_dialog_title(), ssh_test_account("guitest-dialog"), wxString::FromUTF8(probe_dir()));
    dialog.ShowModal();

    sftp().remove_file(probe_dir() + "/unter/drin.txt");
    sftp().remove_dir(probe_dir() + "/unter");
}

TEST_F(SshGuiTest, SshVerzeichnisdialogZeigtFehlerBeiUnbekanntemPfad)
{
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); }));
        ASSERT_TRUE(replace_text(find(dialog, ID_SSH_DIR_PATH_BOX), wxString::FromUTF8(probe_dir() + "/gibt-es-nicht")));
        ASSERT_TRUE(press_return());
        EXPECT_TRUE(wait_until([&dialog] { return status_of(dialog).Matches(translation::translate("error_with_reason", {L"*"})) || status_of(dialog) == translation::translate("not_a_directory"); }))
            << "Der unbekannte Pfad blieb ohne Fehlermeldung, Status: " << status_of(dialog);
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&main_window(), dir_dialog_title(), ssh_test_account("guitest-dialog"), wxString::FromUTF8(probe_dir()));
    dialog.ShowModal();
}

TEST_F(SshGuiTest, SshButtonOeffnetAuswahlUndVerzeichnisdialogUndSetztPfad)  // [SshSyncTest.md#ssh-button]
{
    ASSERT_TRUE(add_test_account(main_window()));
    ASSERT_TRUE(sftp().make_dir(probe_dir() + "/ziel"));
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [this](wxDialog& dialog) {
        ASSERT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); }));
        ASSERT_TRUE(replace_text(find(dialog, ID_TARGET_DIR_BOX), wxString::FromUTF8(":" + probe_dir())));  // [SshSyncTest.md#start-path]
        ASSERT_TRUE(click_on(find(dialog, ID_BTN_SSH_TARGET)));
        EXPECT_EQ(value_of(find(dialog, ID_TARGET_DIR_BOX)), wxString::FromUTF8(std::string(ACCOUNT.ToUTF8()) + ":" + probe_dir() + "/ziel"))
            << "Das Ziel wurde nicht aus Verbindung und gewähltem Verzeichnis zusammengesetzt.";
        dialog.EndModal(wxID_CANCEL);
    });
    ModalDialogWatcher::instance().expect(select_dialog_title(), [](wxDialog& dialog) {
        ASSERT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); }));
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_SELECT_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_GE(row_named(*list, ACCOUNT), 0) << "Die Verbindung fehlt in der Auswahl.";
        EXPECT_FALSE(find(dialog, wxID_OK)->IsEnabled()) << "OK ist ohne Auswahl aktiv.";
        ASSERT_TRUE(click_on(*list, row_named(*list, ACCOUNT)));
        ASSERT_TRUE(click_when_enabled(find(dialog, wxID_OK)));
    });
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(list_shows_file(dialog, "[ziel]")) << "Der Verzeichnisdialog zeigt das Startverzeichnis nicht.";
        ASSERT_TRUE(click_on(*dir_list(dialog), row_named(*dir_list(dialog), "[ziel]")));
        ASSERT_TRUE(click_when_enabled(find(dialog, wxID_OK)));
    });

    activate_menu_item(main_window(), tul(id::File_new_Synchronization));
    pump();

    sftp().remove_dir(probe_dir() + "/ziel");
}

TEST_F(SshGuiTest, EinmalSynchronisierenNachSshLegtDateiAufServerAb)  // [SshSyncTest.md#sync-to-server]
{
    ASSERT_TRUE(add_test_account(main_window()));
    const SandboxTree tree(GuiTestSandbox::root() / "nach-ssh", {{"quelle/bericht.txt", "Bericht"}});
    const std::string remote_target = probe_dir() + "/ziel";
    SyncItemInput input{"guitest-nach-ssh", tree.path("quelle").string(), wxString::FromUTF8(std::string(ACCOUNT.ToUTF8()) + ":" + remote_target)};
    input.type = SyncItemType::Dir;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(sftp().read_file(remote_target + "/bericht.txt"), "Bericht") << "Die Datei kam nicht auf dem Server an.";
    sftp().remove_file(remote_target + "/bericht.txt");
    sftp().remove_dir(remote_target);
}

}
