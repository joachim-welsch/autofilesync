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
#include "GoogleAuth.h"
#include "GoogleDriveAccountDialog.h"
#include "HttpClient.h"
#include "SshDirDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/listctrl.h>
#include <memory>
#include <string>
#include <thread>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString ACCOUNT_NAME = "guitest-google";
constexpr int LOGIN_TIMEOUT_MILLISECONDS = 10000;

class GoogleDriveAccountTest: public GuiTest {  // [GoogleDriveAccountTest.md#GoogleDriveAccountTest]
protected:
    google_auth::Settings real_settings_ = google_auth::settings();
    std::unique_ptr<tu::GoogleDriveTestServer> server_;
    std::thread browser_;

    void use_test_server()
    {
        server_ = std::make_unique<tu::GoogleDriveTestServer>();
        google_auth::settings() = {tu::GoogleDriveTestServer::CLIENT_ID, tu::GoogleDriveTestServer::CLIENT_SECRET,
            server_->url() + "/auth", server_->url() + "/token", server_->url()};
        GoogleDriveAccountDialog::open_browser_with([this](const wxString& url) {  // [GoogleDriveAccountTest.md#browser]
            browser_ = std::thread(tu::visit_like_a_browser, std::string(url.ToUTF8()));
        });
    }

    void TearDown() override
    {
        if (browser_.joinable())
            browser_.join();
        GoogleDriveAccountDialog::open_browser_with([](const wxString& url) { wxLaunchDefaultBrowser(url); });
        google_auth::settings() = real_settings_;
        server_.reset();
        GuiTest::TearDown();
    }

    static std::optional<SshAccount> saved()
    {
        return Config::instance().find_ssh_account_by_name(ACCOUNT_NAME.ToStdString());
    }
};

}

TEST_F(GoogleDriveAccountTest, OhneKennungSagtDasProgrammDassGoogleDriveFehlt)
{
    google_auth::settings().client_id.clear();
    ModalDialogWatcher::instance().answer(translation::translate("info"), wxID_OK);
    activate_menu_item(main_window(), tul(id::File_new_GoogleDriveConnection));
    pump();
    const auto& messages = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front(), wxString(translation::translate("google_drive_not_configured")));
}

TEST_F(GoogleDriveAccountTest, AnmeldungImBrowserSpeichertDieVerbindungUndZeigtIhreOrdner)  // [GoogleDriveAccountTest.md#login]
{
    use_test_server();
    if (!server_->running())
        GTEST_SKIP() << "Der Google-Drive-Testserver ließ sich nicht starten (braucht python3).";
    ModalDialogWatcher::instance().expect(txt::add_google_drive_connection(), [](wxDialog& dialog) {
        ASSERT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); }));
        pump();
        EXPECT_FALSE(find(dialog, wxID_OK)->IsEnabled()) << "OK ist ohne Anmeldung bedienbar.";
        ASSERT_TRUE(replace_text(find(dialog, ID_GOOGLE_NAME_BOX), ACCOUNT_NAME));
        ASSERT_TRUE(click_on(find(dialog, ID_GOOGLE_LOGIN_BUTTON)));
        EXPECT_TRUE(click_when_enabled(find(dialog, wxID_OK), LOGIN_TIMEOUT_MILLISECONDS)) << "Die Anmeldung kam nicht an.";
        pump();
        if (dialog.IsModal())
            dialog.EndModal(wxID_CANCEL);
    });
    activate_menu_item(main_window(), tul(id::File_new_GoogleDriveConnection));
    pump();
    const auto account = saved();
    ASSERT_TRUE(account.has_value()) << "Die Verbindung wurde nicht gespeichert.";
    EXPECT_TRUE(account->google_drive());
    EXPECT_FALSE(account->webdav());
    EXPECT_EQ(account->password(), std::string(tu::GoogleDriveTestServer::REFRESH_TOKEN));
    EXPECT_EQ(account->url(), server_->url());

    http::Request put(server_->url() + "/__test/files/fotos/urlaub.jpg", "PUT");
    put.body("jpeg");
    put.expect_success(put.perform());
    ModalDialogWatcher::instance().expect(txt::select_ssh_dir(), [](wxDialog& dialog) {
        auto* list = static_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
        EXPECT_TRUE(wait_until([list] { return list->FindItem(-1, "[fotos]") != wxNOT_FOUND; })) << "Der Ordner im Drive erscheint nicht.";
        dialog.EndModal(wxID_CANCEL);
    });
    SshDirDialog dir_dialog(&main_window(), txt::select_ssh_dir(), *account, "/");
    dir_dialog.ShowModal();
}

}
