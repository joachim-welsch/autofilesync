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
    void populateList();
    void updateButtons();
    
    void onAdd(wxCommandEvent& event);
    void onEdit(wxCommandEvent& event);
    void onDelete(wxCommandEvent& event);
    void onListDClick(wxMouseEvent& event);
    void onListSelect(wxListEvent& event);
    void onListDeselect(wxListEvent& event);
    void onEndLabelEdit(wxListEvent& event);
    
    void editSelectedItem();
    long getSelectedIndex() const;

    wxListCtrl* listCtrl_ = nullptr;
    wxButton* addButton_ = nullptr;
    wxButton* editButton_ = nullptr;
    wxButton* deleteButton_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSHACCOUNTS_LIST = wxID_HIGHEST + 200,
    ID_SSHACCOUNTS_ADD,
    ID_SSHACCOUNTS_EDIT,
    ID_SSHACCOUNTS_DELETE
};
