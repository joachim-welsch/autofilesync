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
	#include <wx/wx.h>
#endif
#include "wx/spinctrl.h"
#include "wx/notebook.h"

class LoggingDialog: public wxDialog {
public:
    LoggingDialog(wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~LoggingDialog();

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    wxPanel* create_logging_page(wxNotebook* notebook);
    wxPanel* create_email_page(wxNotebook* notebook);
    void add_field(wxPanel* page, wxSizer* page_sizer, const wxString& label,
        wxWindowID id, wxTextCtrl*& box, long style = 0);
    void on_notify_active_changed(wxCommandEvent&);
    void on_pick_backup_proof_path(wxCommandEvent&);
    void update_notify_enabling();

    wxTextCtrl* log_path_box_ = nullptr;
    wxChoice*   log_level_choice_ = nullptr;
    wxSpinCtrl* retention_days_ctrl_ = nullptr;
    wxTextCtrl* edit_path_box_ = nullptr;
    wxTextCtrl* backup_proof_path_box_ = nullptr;
    wxCheckBox* email_active_check_ = nullptr;
    wxTextCtrl* email_box_ = nullptr;
    wxTextCtrl* smtp_server_box_ = nullptr;  // the server the notification is sent through
    wxSpinCtrl* smtp_port_ctrl_ = nullptr;
    wxTextCtrl* smtp_user_box_ = nullptr;
    wxTextCtrl* smtp_password_box_ = nullptr;
    wxTextCtrl* smtp_from_box_ = nullptr;

DECLARE_EVENT_TABLE()
};

enum {
    ID_LOG_PATH_BOX = wxID_HIGHEST + 1,
    ID_LOG_LEVEL_CHOICE,
    ID_RETENTIONDAYS_SPIN,
    ID_EDIT_PATH_BOX,
    ID_BACKUP_PROOF_PATH_BOX,
    ID_BTN_PICK_BACKUP_PROOF_PATH,
    ID_EMAIL_ACTIVE_CHECK,
    ID_EMAIL_BOX,
    ID_SMTP_SERVER_BOX,
    ID_SMTP_PORT_SPIN,
    ID_SMTP_USER_BOX,
    ID_SMTP_PASSWORD_BOX,
    ID_SMTP_FROM_BOX
};
