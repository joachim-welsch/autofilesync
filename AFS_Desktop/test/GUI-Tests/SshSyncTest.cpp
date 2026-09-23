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
#include <wx/uiaction.h>

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

bool addTestAccount(MainWindow& window)
{
    const SshAccountInput input{ACCOUNT, SSH_TEST_HOST, SSH_TEST_USER, SSH_TEST_PASSWORD};
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(input));
    activateMenuItem(window, tul(id::File_new_SshConnection));
    pump();
    return Config::instance().find_ssh_account_by_name(std::string(ACCOUNT.ToUTF8())).has_value();
}

wxListCtrl* dirList(wxDialog& dialog)
{
    return dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
}

long rowNamed(wxListCtrl& list, const wxString& text)
{
    for (long row = 0; row < list.GetItemCount(); ++row)
        if (list.GetItemText(row) == text)
            return row;
    return -1;
}

wxString statusOf(wxDialog& dialog)  // [SshSyncTest.md#statusOf]
{
    const wxString connectionLabel = wxString(translation::translate("ssh_connection_info")).BeforeFirst('%');
    for (wxWindow* child : dialog.GetChildren())
        if (auto* text = dynamic_cast<wxStaticText*>(child); text != nullptr && text->GetId() == wxID_STATIC && !text->GetLabelText().StartsWith(connectionLabel))
            return text->GetLabelText();
    return wxString();
}

bool listShowsFile(wxDialog& dialog, const wxString& name)
{
    return waitUntil([&dialog, name] { wxListCtrl* list = dirList(dialog); return list != nullptr && rowNamed(*list, name) >= 0; });
}

}

TEST_F(SshGuiTest, SshVerzeichnisdialogDoppelklickSteigtAb)
{
    ASSERT_TRUE(sftp().makeDir(probeDir() + "/unter"));
    ASSERT_TRUE(sftp().createFile(probeDir() + "/unter/drin.txt"));
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(listShowsFile(dialog, "[unter]")) << "Das Unterverzeichnis erscheint nicht in der Liste.";
        ASSERT_TRUE(clickOn(*dirList(dialog), rowNamed(*dirList(dialog), "[unter]"), true));
        EXPECT_TRUE(listShowsFile(dialog, "drin.txt")) << "Der Doppelklick stieg nicht ins Verzeichnis ab.";
        EXPECT_EQ(valueOf(find(dialog, ID_SSH_DIR_PATH_BOX)), wxString::FromUTF8(probeDir() + "/unter"));
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&mainWindow(), dir_dialog_title(), sshTestAccount("guitest-dialog"), wxString::FromUTF8(probeDir()));
    dialog.ShowModal();

    sftp().removeFile(probeDir() + "/unter/drin.txt");
    sftp().removeDir(probeDir() + "/unter");
}

TEST_F(SshGuiTest, SshVerzeichnisdialogPfadfeldMitEnterNavigiert)
{
    ASSERT_TRUE(sftp().makeDir(probeDir() + "/unter"));
    ASSERT_TRUE(sftp().createFile(probeDir() + "/unter/drin.txt"));
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(listShowsFile(dialog, "[unter]"));
        ASSERT_TRUE(replaceText(find(dialog, ID_SSH_DIR_PATH_BOX), wxString::FromUTF8(probeDir() + "/unter")));
        wxUIActionSimulator simulator;
        ASSERT_TRUE(simulator.Char(WXK_RETURN));
        EXPECT_TRUE(listShowsFile(dialog, "drin.txt")) << "Enter im Pfadfeld wechselte nicht ins Verzeichnis.";
        EXPECT_TRUE(statusOf(dialog).Matches(translation::translate("entries_count", {L"*"}))) << statusOf(dialog);
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&mainWindow(), dir_dialog_title(), sshTestAccount("guitest-dialog"), wxString::FromUTF8(probeDir()));
    dialog.ShowModal();

    sftp().removeFile(probeDir() + "/unter/drin.txt");
    sftp().removeDir(probeDir() + "/unter");
}

TEST_F(SshGuiTest, SshVerzeichnisdialogZeigtFehlerBeiUnbekanntemPfad)
{
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(waitUntil([&dialog] { return dialog.IsShownOnScreen(); }));
        ASSERT_TRUE(replaceText(find(dialog, ID_SSH_DIR_PATH_BOX), wxString::FromUTF8(probeDir() + "/gibt-es-nicht")));
        wxUIActionSimulator simulator;
        ASSERT_TRUE(simulator.Char(WXK_RETURN));
        EXPECT_TRUE(waitUntil([&dialog] { return statusOf(dialog).Matches(translation::translate("error_with_reason", {L"*"})) || statusOf(dialog) == translation::translate("not_a_directory"); }))
            << "Der unbekannte Pfad blieb ohne Fehlermeldung, Status: " << statusOf(dialog);
        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dialog(&mainWindow(), dir_dialog_title(), sshTestAccount("guitest-dialog"), wxString::FromUTF8(probeDir()));
    dialog.ShowModal();
}

TEST_F(SshGuiTest, SshButtonOeffnetAuswahlUndVerzeichnisdialogUndSetztPfad)  // [SshSyncTest.md#ssh-button]
{
    ASSERT_TRUE(addTestAccount(mainWindow()));
    ASSERT_TRUE(sftp().makeDir(probeDir() + "/ziel"));
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [this](wxDialog& dialog) {
        ASSERT_TRUE(waitUntil([&dialog] { return dialog.IsShownOnScreen(); }));
        ASSERT_TRUE(replaceText(find(dialog, ID_TARGET_DIR_BOX), wxString::FromUTF8(":" + probeDir())));  // [SshSyncTest.md#start-path]
        ASSERT_TRUE(clickOn(find(dialog, ID_BTN_SSH_TARGET)));
        EXPECT_EQ(valueOf(find(dialog, ID_TARGET_DIR_BOX)), wxString::FromUTF8(std::string(ACCOUNT.ToUTF8()) + ":" + probeDir() + "/ziel"))
            << "Das Ziel wurde nicht aus Verbindung und gewähltem Verzeichnis zusammengesetzt.";
        dialog.EndModal(wxID_CANCEL);
    });
    ModalDialogWatcher::instance().expect(select_dialog_title(), [](wxDialog& dialog) {
        ASSERT_TRUE(waitUntil([&dialog] { return dialog.IsShownOnScreen(); }));
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_SELECT_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_GE(rowNamed(*list, ACCOUNT), 0) << "Die Verbindung fehlt in der Auswahl.";
        EXPECT_FALSE(find(dialog, wxID_OK)->IsEnabled()) << "OK ist ohne Auswahl aktiv.";
        ASSERT_TRUE(clickOn(*list, rowNamed(*list, ACCOUNT)));
        ASSERT_TRUE(clickWhenEnabled(find(dialog, wxID_OK)));
    });
    ModalDialogWatcher::instance().expect(dir_dialog_title(), [this](wxDialog& dialog) {
        ASSERT_TRUE(listShowsFile(dialog, "[ziel]")) << "Der Verzeichnisdialog zeigt das Startverzeichnis nicht.";
        ASSERT_TRUE(clickOn(*dirList(dialog), rowNamed(*dirList(dialog), "[ziel]")));
        ASSERT_TRUE(clickWhenEnabled(find(dialog, wxID_OK)));
    });

    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();

    sftp().removeDir(probeDir() + "/ziel");
}

TEST_F(SshGuiTest, EinmalSynchronisierenNachSshLegtDateiAufServerAb)  // [SshSyncTest.md#sync-to-server]
{
    ASSERT_TRUE(addTestAccount(mainWindow()));
    const SandboxTree tree(GuiTestSandbox::root() / "nach-ssh", {{"quelle/bericht.txt", "Bericht"}});
    const std::string remoteTarget = probeDir() + "/ziel";
    SyncItemInput input{"guitest-nach-ssh", tree.path("quelle").string(), wxString::FromUTF8(std::string(ACCOUNT.ToUTF8()) + ":" + remoteTarget)};
    input.type = SyncItemType::Dir;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(sftp().readFile(remoteTarget + "/bericht.txt"), "Bericht") << "Die Datei kam nicht auf dem Server an.";
    sftp().removeFile(remoteTarget + "/bericht.txt");
    sftp().removeDir(remoteTarget);
}

}
