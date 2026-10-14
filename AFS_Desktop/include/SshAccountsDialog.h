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

#include <wx/listctrl.h>

class SshAccountsDialog : public wxDialog {
public:
    SshAccountsDialog(wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SshAccountsDialog();

private:
    void populate_list();
    void update_buttons();
    
    void on_add(wxCommandEvent& event);
    void on_edit(wxCommandEvent& event);
    void on_delete(wxCommandEvent& event);
    void on_list_double_click(wxMouseEvent& event);
    void on_list_select(wxListEvent& event);
    void on_list_deselect(wxListEvent& event);
    void on_end_label_edit(wxListEvent& event);
    
    void edit_selected_item();
    long get_selected_index() const;

    wxListCtrl* list_ctrl_ = nullptr;
    wxButton* add_button_ = nullptr;
    wxButton* edit_button_ = nullptr;
    wxButton* delete_button_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSHACCOUNTS_LIST = wxID_HIGHEST + 200,
    ID_SSHACCOUNTS_ADD,
    ID_SSHACCOUNTS_EDIT,
    ID_SSHACCOUNTS_DELETE
};
