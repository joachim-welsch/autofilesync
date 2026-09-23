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
#include "SshAccountDialogDriver.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "SshAccountDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString ACCOUNT = "guitest-konto";
wxString validation_error()  // translated at the call, once the catalogue is bound
{
    return translation::translate("validation_error");
}

SshAccountInput account(const wxString& name = ACCOUNT)
{
    return SshAccountInput{name, "rechner.example.org", "benutzer", "geheim"};
}

bool addAccount(MainWindow& window, const SshAccountInput& input)  // [SshAccountsTest.md#addAccount]
{
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(input));
    activateMenuItem(window, tul(id::File_new_SshConnection));
    pump();
    return Config::instance().find_ssh_account_by_name(std::string(input.name.ToUTF8())).has_value();
}

void openAccountsDialog(MainWindow& window)
{
    activateMenuItem(window, tul(id::EditSshAccounts));
    pump();
}

std::optional<SshAccount> accountNamed(const wxString& name)
{
    return Config::instance().find_ssh_account_by_name(std::string(name.ToUTF8()));
}

}

TEST_F(GuiTest, SshVerbindungBearbeitenAendertHost)
{
    ASSERT_TRUE(addAccount(mainWindow(), account()));
    SshAccountInput edited = account();
    edited.host = "anderer.example.org";
    ModalDialogWatcher::instance().expect(txt::ssh_connections(), editSshAccountAndClose(ACCOUNT));
    ModalDialogWatcher::instance().expect(txt::edit_ssh_connection(), fillSshAccountDialog(edited));

    openAccountsDialog(mainWindow());

    const auto changed = accountNamed(ACCOUNT);
    ASSERT_TRUE(changed.has_value());
    EXPECT_EQ(changed->host(), "anderer.example.org") << "Der neue Host kam nicht in die Konfiguration.";
    EXPECT_EQ(changed->user(), "benutzer") << "Der Benutzer ging beim Bearbeiten verloren.";
}

TEST_F(GuiTest, SshVerbindungLoeschenFragtNachUndWarntBeiVerwendung)  // [SshAccountsTest.md#delete-warning]
{
    ASSERT_TRUE(addAccount(mainWindow(), account()));
    SyncItemInput item{"guitest-nutzt-konto", GuiTestSandbox::sourceDir().string(), ACCOUNT + ":/daten"};
    item.type = SyncItemType::Dir;
    ASSERT_TRUE(addSyncItem(mainWindow(), item));
    ModalDialogWatcher::instance().expect(txt::ssh_connections(), deleteSshAccountAndClose(ACCOUNT));
    ModalDialogWatcher::instance().answer(translation::translate("confirm_delete_title"), wxID_YES);

    openAccountsDialog(mainWindow());

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Es wurde ohne Rückfrage gelöscht.";
    const wxString& question = ModalDialogWatcher::instance().answeredMessages().front();
    EXPECT_TRUE(question.Contains(ACCOUNT)) << question;
    EXPECT_TRUE(question.Contains(translation::translate("ssh_account_in_use_warning"))) << "Die Rückfrage warnt nicht vor der Verwendung: " << question;
    EXPECT_FALSE(accountNamed(ACCOUNT).has_value()) << "Die Verbindung steht noch in der Konfiguration.";
}

TEST_F(GuiTest, SshVerbindungLoeschenOhneVerwendungWarntNicht)  // [SshAccountsTest.md#delete-warning]
{
    ASSERT_TRUE(addAccount(mainWindow(), account()));
    ModalDialogWatcher::instance().expect(txt::ssh_connections(), deleteSshAccountAndClose(ACCOUNT));
    ModalDialogWatcher::instance().answer(translation::translate("confirm_delete_title"), wxID_NO);

    openAccountsDialog(mainWindow());

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u);
    EXPECT_FALSE(ModalDialogWatcher::instance().answeredMessages().front().Contains(translation::translate("ssh_account_in_use_warning")));
    EXPECT_TRUE(accountNamed(ACCOUNT).has_value()) << "Trotz Nein wurde gelöscht.";
}

TEST_F(GuiTest, SshVerbindungUmbenennenAktualisiertVerweise)  // [SshAccountsTest.md#rename]
{
    ASSERT_TRUE(addAccount(mainWindow(), account()));
    SyncItemInput item{"guitest-nutzt-konto", GuiTestSandbox::sourceDir().string(), ACCOUNT + ":/daten"};
    item.type = SyncItemType::Dir;
    ASSERT_TRUE(addSyncItem(mainWindow(), item));
    ModalDialogWatcher::instance().expect(txt::ssh_connections(), renameSshAccountAndClose(ACCOUNT, "guitest-neu"));

    openAccountsDialog(mainWindow());

    EXPECT_FALSE(accountNamed(ACCOUNT).has_value()) << "Der alte Name blieb bestehen.";
    ASSERT_TRUE(accountNamed("guitest-neu").has_value()) << "Der neue Name kam nicht in die Konfiguration.";
    const auto renamedItem = Config::instance().find_by_name(L"guitest-nutzt-konto");
    ASSERT_TRUE(renamedItem.has_value());
    EXPECT_EQ(renamedItem->target(), L"guitest-neu:/daten") << "Der Verweis in der Synchronisation wurde nicht mitgezogen.";
}

TEST_F(GuiTest, SshNameZuKurzWirdAbgelehnt)  // [SshAccountsTest.md#rejected-input]
{
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(account("x")));
    ModalDialogWatcher::instance().answer(validation_error(), wxID_OK);

    activateMenuItem(mainWindow(), tul(id::File_new_SshConnection));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der einbuchstabige Name wurde angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("ssh_name_too_short"));
    EXPECT_TRUE(Config::instance().ssh_accounts_snapshot().empty());
}

TEST_F(GuiTest, SshNameMitDoppelpunktWirdAbgelehnt)  // [SshAccountsTest.md#rejected-input]
{
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(account("konto:eins")));
    ModalDialogWatcher::instance().answer(validation_error(), wxID_OK);

    activateMenuItem(mainWindow(), tul(id::File_new_SshConnection));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der Name mit Doppelpunkt wurde angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("ssh_name_colon"));
    EXPECT_TRUE(Config::instance().ssh_accounts_snapshot().empty());
}

TEST_F(GuiTest, SshNameMitSonderzeichenWirdAbgelehnt)  // [SshAccountsTest.md#rejected-input]
{
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(account("konto eins")));
    ModalDialogWatcher::instance().answer(validation_error(), wxID_OK);

    activateMenuItem(mainWindow(), tul(id::File_new_SshConnection));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der Name mit Leerzeichen wurde angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("ssh_name_characters"));
    EXPECT_TRUE(Config::instance().ssh_accounts_snapshot().empty());
}

TEST_F(GuiTest, SshDoppelterNameWirdAbgelehnt)  // [SshAccountsTest.md#rejected-input]
{
    ASSERT_TRUE(addAccount(mainWindow(), account()));
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fillSshAccountDialog(account()));
    ModalDialogWatcher::instance().answer(validation_error(), wxID_OK);

    activateMenuItem(mainWindow(), tul(id::File_new_SshConnection));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der doppelte Name wurde angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), translation::translate("ssh_account_exists", {ACCOUNT.ToStdWstring()}));
    EXPECT_EQ(Config::instance().ssh_accounts_snapshot().size(), 1u);
}

TEST_F(GuiTest, SshOkNurMitHostUndBenutzer)
{
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), [](wxDialog& dialog) {
        ASSERT_TRUE(waitUntil([&dialog] { return dialog.IsShownOnScreen(); }));
        wxWindow* ok = find(dialog, wxID_OK);
        ASSERT_TRUE(replaceText(find(dialog, ID_SSH_NAME_BOX), ACCOUNT));
        pump();
        EXPECT_FALSE(ok->IsEnabled()) << "OK ist ohne Host und Benutzer aktiv.";
        ASSERT_TRUE(replaceText(find(dialog, ID_SSH_HOST_BOX), "rechner.example.org"));
        pump();
        EXPECT_FALSE(ok->IsEnabled()) << "OK ist ohne Benutzer aktiv.";
        ASSERT_TRUE(replaceText(find(dialog, ID_SSH_USER_BOX), "benutzer"));
        pump();
        EXPECT_TRUE(ok->IsEnabled()) << "OK bleibt mit Host und Benutzer gesperrt.";
        dialog.EndModal(wxID_CANCEL);
    });

    activateMenuItem(mainWindow(), tul(id::File_new_SshConnection));
    pump();

    EXPECT_TRUE(Config::instance().ssh_accounts_snapshot().empty());
}

}
