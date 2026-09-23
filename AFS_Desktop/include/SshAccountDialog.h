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
#include <optional>

namespace AFS_Core { class SshAccount; }

using namespace AFS_Core;

std::optional<wxString> validateSshAccountName(const wxString& name);  // [SshAccountDialog.md#validateSshAccountName]

class SshAccountDialog : public wxDialog {
public:
    SshAccountDialog(SshAccount* sshAccount,
        wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SshAccountDialog();

    wxString name() const { return name_; }
    wxString user() const { return user_; }
    wxString host() const { return host_; }
    wxString password() const { return password_; }

    void saveData(SshAccount& account);

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    void onUpdateOkButton(wxUpdateUIEvent& event);
    void setData(const SshAccount& account);

    wxString name_;
    wxString user_;
    wxString host_;
    wxString password_;

    wxTextCtrl* nameBox_ = nullptr;
    wxTextCtrl* userBox_ = nullptr;
    wxTextCtrl* hostBox_ = nullptr;
    wxTextCtrl* passwordBox_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_NAME_BOX = wxID_HIGHEST + 100,
    ID_SSH_USER_BOX,
    ID_SSH_HOST_BOX,
    ID_SSH_PASSWORD_BOX
};
