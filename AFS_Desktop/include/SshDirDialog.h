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
#include <wx/treectrl.h>
#include <string>
#include <optional>
#include <vector>
#include "SshAccount.h"
#include "SshDirInspector.h"

using namespace AFS_Core;

class SshDirDialog : public wxDialog {
public:
    SshDirDialog(wxWindow* parent, 
        const wxString& title,
        const SshAccount& account,
        const wxString& default_path = "/",
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~SshDirDialog();

    wxString get_path() const { return selected_path_; }
    const SshAccount& get_account() const { return account_; }

private:
    void populate_list(const wxString& path);
    void render_list();
    void on_list_double_click(wxMouseEvent& event);
    void on_list_select(wxListEvent& event);
    void on_path_enter(wxCommandEvent& event);
    void on_home_button(wxCommandEvent& event);
    void on_sort_by_name(wxCommandEvent& event);

    SshAccount account_;
    wxString current_path_;
    wxString selected_path_;
    std::vector<RemoteDirEntry> entries_;
    bool sort_ascending_ = true;

    wxTextCtrl* path_box_ = nullptr;
    wxListCtrl* list_ctrl_ = nullptr;
    wxButton* home_button_ = nullptr;
    wxButton* sort_button_ = nullptr;
    wxStaticText* status_text_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_DIR_PATH_BOX = wxID_HIGHEST + 300,
    ID_SSH_DIR_LIST,
    ID_SSH_DIR_HOME_BUTTON,
    ID_SSH_DIR_SORT_BUTTON
};
