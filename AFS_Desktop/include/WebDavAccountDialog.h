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

namespace AFS_Core { class SshAccount; }

class WebDavAccountDialog : public wxDialog {  // [WebDavAccountDialog.md#WebDavAccountDialog]
public:
    WebDavAccountDialog(const AFS_Core::SshAccount& account, wxWindow* parent, wxWindowID id, const wxString& title);
    void save_data(AFS_Core::SshAccount& account) const;
private:
    bool TransferDataFromWindow() override;
    bool Validate() override;
    bool name_valid();
    bool address_valid();
    bool server_answers();  // [WebDavAccountDialog.md#connection-check]
    wxTextCtrl* add_field(wxSizer* sizer, wxWindowID id, const char* label_msgid, const wxString& value, long style = 0);
    void on_update_ok_button(wxUpdateUIEvent& event);

    wxString original_name_;
    wxString name_;
    wxString url_;
    wxString user_;
    wxString password_;
    wxTextCtrl* name_box_ = nullptr;
    wxTextCtrl* url_box_ = nullptr;
    wxTextCtrl* user_box_ = nullptr;
    wxTextCtrl* password_box_ = nullptr;
};

enum {
    ID_WEBDAV_NAME_BOX = wxID_HIGHEST + 370,
    ID_WEBDAV_URL_BOX,
    ID_WEBDAV_USER_BOX,
    ID_WEBDAV_PASSWORD_BOX
};
