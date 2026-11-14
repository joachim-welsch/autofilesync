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
#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/timer.h>
#include <functional>
#include <memory>
#include <string>
#include "GoogleAuth.h"

namespace AFS_Core { class SshAccount; }

class GoogleDriveAccountDialog : public wxDialog {  // [GoogleDriveAccountDialog.md#GoogleDriveAccountDialog]
public:
    using BrowserOpener = std::function<void(const wxString& url)>;
    GoogleDriveAccountDialog(const AFS_Core::SshAccount& account, wxWindow* parent, wxWindowID id, const wxString& title);
    void save_data(AFS_Core::SshAccount& account) const;
    static void open_browser_with(BrowserOpener opener);  // [GoogleDriveAccountDialog.md#open_browser_with]
private:
    bool TransferDataFromWindow() override;
    bool Validate() override;
    void on_login(wxCommandEvent& event);
    void on_poll(wxTimerEvent& event);  // [GoogleDriveAccountDialog.md#polling]
    void on_update_ok_button(wxUpdateUIEvent& event);
    void show_status(const wxString& text);

    wxString original_name_;
    wxString name_;
    std::string refresh_token_;
    std::unique_ptr<AFS_Core::google_auth::Login> login_;
    wxTimer poll_timer_;
    wxTextCtrl* name_box_ = nullptr;
    wxButton* login_button_ = nullptr;
    wxStaticText* status_ = nullptr;
};

enum {
    ID_GOOGLE_NAME_BOX = wxID_HIGHEST + 380,
    ID_GOOGLE_LOGIN_BUTTON,
    ID_GOOGLE_STATUS
};
