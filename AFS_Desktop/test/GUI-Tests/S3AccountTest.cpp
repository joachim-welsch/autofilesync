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
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "WebDavTestServer.h"
#include "Config.h"
#include "S3AccountDialog.h"
#include "SshDirDialog.h"
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

const wxString ACCOUNT_NAME = "guitest-s3";

class S3AccountTest: public GuiTest {  // [S3AccountTest.md#S3AccountTest]
protected:
    void add_connection(const wxString& url, const wxString& secret, const wxString& answered_dialog, int answer)  // [WebDavAccountTest.md#add_connection]
    {
        ModalDialogWatcher::instance().expect(txt::add_s3_connection(), [=](wxDialog& dialog) {
            if (!answered_dialog.empty())
                ModalDialogWatcher::instance().answer(answered_dialog, answer);
            const bool filled = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
                && replace_text(find(dialog, ID_S3_NAME_BOX), ACCOUNT_NAME)
                && replace_text(find(dialog, ID_S3_URL_BOX), url)
                && replace_text(find(dialog, ID_S3_REGION_BOX), tu::S3TestServer::REGION)
                && replace_text(find(dialog, ID_S3_ACCESS_KEY_BOX), tu::S3TestServer::ACCESS_KEY)
                && replace_text(find(dialog, ID_S3_SECRET_KEY_BOX), secret)
                && click_when_enabled(find(dialog, wxID_OK));
            EXPECT_TRUE(filled) << "Der Dialog ließ sich nicht ausfüllen.";
            pump();
            if (dialog.IsShown())  // [ModalDialogWatcher.md#left-open]
                dialog.EndModal(wxID_CANCEL);
        });
        activate_menu_item(main_window(), tul(id::File_new_S3Connection));
        pump();
    }

    static std::optional<SshAccount> saved()
    {
        return Config::instance().find_ssh_account_by_name(ACCOUNT_NAME.ToStdString());
    }
};

}

TEST_F(S3AccountTest, FalscherSchluesselWirdOhneZustimmungNichtGespeichert)
{
    const tu::S3TestServer server;
    if (!server.running())
        GTEST_SKIP() << "Der S3-Testserver ließ sich nicht starten (braucht python3).";
    add_connection(server.url(), "falsch", translation::translate("webdav_check_caption"), wxID_NO);
    EXPECT_FALSE(saved().has_value());
}

TEST_F(S3AccountTest, GepruefteVerbindungWirdGespeichertUndZeigtDieSpeicherbereiche)  // [S3AccountTest.md#buckets]
{
    const tu::S3TestServer server;
    if (!server.running())
        GTEST_SKIP() << "Der S3-Testserver ließ sich nicht starten (braucht python3).";
    add_connection(server.url(), tu::S3TestServer::SECRET_KEY, wxEmptyString, wxID_NONE);
    const auto account = saved();
    ASSERT_TRUE(account.has_value()) << "Die geprüfte Verbindung wurde nicht gespeichert.";
    EXPECT_TRUE(account->s3());
    EXPECT_EQ(account->region(), std::string(tu::S3TestServer::REGION));
    EXPECT_EQ(account->password(), std::string(tu::S3TestServer::SECRET_KEY));

    ModalDialogWatcher::instance().expect(txt::select_ssh_dir(), [](wxDialog& dialog) {
        auto* list = static_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
        const wxString bucket = wxString("[") + tu::S3TestServer::BUCKET + "]";
        EXPECT_TRUE(wait_until([list, bucket] { return list->FindItem(-1, bucket) != wxNOT_FOUND; })) << "Der Speicherbereich erscheint nicht.";
        dialog.EndModal(wxID_CANCEL);
    });
    SshDirDialog dir_dialog(&main_window(), txt::select_ssh_dir(), *account, "/");
    dir_dialog.ShowModal();
}

}
