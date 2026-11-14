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
#include "UserActions.h"
#include "WebDavTestServer.h"
#include "Config.h"
#include "SshDirDialog.h"
#include "WebDavAccountDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/listctrl.h>
#include <string>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString ACCOUNT_NAME = "guitest-webdav";
const wxString NOBODY_LISTENS = "http://127.0.0.1:9";  // [WebDavAccountTest.md#NOBODY_LISTENS]

bool fill_and_confirm(wxDialog& dialog, const wxString& url)
{
    return wait_until([&dialog] { return dialog.IsShownOnScreen(); })
        && replace_text(find(dialog, ID_WEBDAV_NAME_BOX), ACCOUNT_NAME)
        && replace_text(find(dialog, ID_WEBDAV_URL_BOX), url)
        && replace_text(find(dialog, ID_WEBDAV_USER_BOX), tu::WebDavTestServer::USER)
        && replace_text(find(dialog, ID_WEBDAV_PASSWORD_BOX), tu::WebDavTestServer::PASSWORD)
        && click_when_enabled(find(dialog, wxID_OK));
}

bool server_dir_shows(wxDialog& dialog, const wxString& name)
{
    auto* list = static_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
    return wait_until([list, &name] { return list->FindItem(-1, name) != wxNOT_FOUND; });
}

class WebDavAccountTest: public GuiTest {  // [WebDavAccountTest.md#WebDavAccountTest]
protected:
    void add_connection(const wxString& url, const wxString& answered_dialog, int answer)  // [WebDavAccountTest.md#add_connection]
    {
        ModalDialogWatcher::instance().expect(txt::add_webdav_connection(), [=](wxDialog& dialog) {
            if (!answered_dialog.empty())
                ModalDialogWatcher::instance().answer(answered_dialog, answer);
            EXPECT_TRUE(fill_and_confirm(dialog, url)) << "Der Dialog ließ sich nicht ausfüllen.";
            pump();
            if (dialog.IsModal())
                dialog.EndModal(wxID_CANCEL);
        });
        activate_menu_item(main_window(), tul(id::File_new_WebDavConnection));
        pump();
    }

    static std::optional<SshAccount> saved()
    {
        return Config::instance().find_ssh_account_by_name(ACCOUNT_NAME.ToStdString());
    }
};

}

TEST_F(WebDavAccountTest, AdresseOhneHttpsWirdNichtAngenommen)
{
    add_connection("http://cloud.example.org/dav", translation::translate("validation_error"), wxID_OK);
    EXPECT_FALSE(saved().has_value()) << "Eine unverschlüsselte Adresse wurde gespeichert.";
    const auto& messages = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front(), wxString(translation::translate("webdav_address_needs_https")));
}

TEST_F(WebDavAccountTest, NichtErreichbarerServerWirdOhneZustimmungNichtGespeichert)
{
    add_connection(NOBODY_LISTENS, translation::translate("webdav_check_caption"), wxID_NO);
    EXPECT_FALSE(saved().has_value());
}

TEST_F(WebDavAccountTest, NichtErreichbarerServerWirdAufWunschTrotzdemGespeichert)
{
    add_connection(NOBODY_LISTENS, translation::translate("webdav_check_caption"), wxID_YES);
    const auto account = saved();
    ASSERT_TRUE(account.has_value());
    EXPECT_TRUE(account->webdav());
    EXPECT_EQ(account->url(), NOBODY_LISTENS.ToStdString());
}

TEST_F(WebDavAccountTest, ErreichbarerServerWirdGespeichertUndZeigtSeineOrdner)  // [WebDavAccountTest.md#with-server]
{
    const SandboxTree server_files(GuiTestSandbox::root() / "webdav-server", {{"bilder/", ""}, {"brief.txt", "hallo"}});
    const tu::WebDavTestServer server(server_files.root(), true);
    if (!server.running())
        GTEST_SKIP() << "Der WebDAV-Testserver ließ sich nicht starten (braucht python3).";
    add_connection(server.url(), wxEmptyString, wxID_NONE);
    const auto account = saved();
    ASSERT_TRUE(account.has_value()) << "Die geprüfte Verbindung wurde nicht gespeichert.";
    EXPECT_EQ(account->password(), std::string(tu::WebDavTestServer::PASSWORD));

    ModalDialogWatcher::instance().expect(txt::select_ssh_dir(), [](wxDialog& dialog) {
        EXPECT_TRUE(server_dir_shows(dialog, "[bilder]")) << "Der Ordner auf dem Server erscheint nicht.";
        dialog.EndModal(wxID_CANCEL);
    });
    SshDirDialog dir_dialog(&main_window(), txt::select_ssh_dir(), *account, "/");
    dir_dialog.ShowModal();
}

}
