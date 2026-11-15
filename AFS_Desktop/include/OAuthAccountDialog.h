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

enum class CloudProvider { GoogleDrive, OneDrive };  // [OAuthAccountDialog.md#CloudProvider]

class OAuthAccountDialog : public wxDialog {  // [OAuthAccountDialog.md#OAuthAccountDialog]
public:
    using BrowserOpener = std::function<void(const wxString& url)>;
    OAuthAccountDialog(CloudProvider provider, const AFS_Core::SshAccount& account, wxWindow* parent, wxWindowID id, const wxString& title);
    static CloudProvider provider_of(const AFS_Core::SshAccount& account);
    static bool configured(CloudProvider provider);
    void save_data(AFS_Core::SshAccount& account) const;
    static void open_browser_with(BrowserOpener opener);  // [OAuthAccountDialog.md#open_browser_with]
private:
    bool TransferDataFromWindow() override;
    bool Validate() override;
    void on_login(wxCommandEvent& event);
    void on_poll(wxTimerEvent& event);  // [OAuthAccountDialog.md#polling]
    void on_update_ok_button(wxUpdateUIEvent& event);
    void show_status(const wxString& text);

    wxString original_name_;
    wxString name_;
    std::string refresh_token_;
    CloudProvider provider_;
    std::unique_ptr<AFS_Core::oauth::Login> login_;
    wxTimer poll_timer_;
    wxTextCtrl* name_box_ = nullptr;
    wxButton* login_button_ = nullptr;
    wxStaticText* status_ = nullptr;
};

enum {
    ID_OAUTH_NAME_BOX = wxID_HIGHEST + 380,
    ID_OAUTH_LOGIN_BUTTON,
    ID_OAUTH_STATUS
};
