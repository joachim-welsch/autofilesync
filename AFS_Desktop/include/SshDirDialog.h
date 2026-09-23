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
        const wxString& defaultPath = "/",
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~SshDirDialog();

    wxString getPath() const { return selectedPath_; }
    const SshAccount& getAccount() const { return account_; }

private:
    void populateList(const wxString& path);
    void renderList();
    void onListDClick(wxMouseEvent& event);
    void onListSelect(wxListEvent& event);
    void onPathEnter(wxCommandEvent& event);
    void onHomeButton(wxCommandEvent& event);
    void onSortByName(wxCommandEvent& event);

    SshAccount account_;
    wxString currentPath_;
    wxString selectedPath_;
    std::vector<RemoteDirEntry> entries_;
    bool sortAscending_ = true;

    wxTextCtrl* pathBox_ = nullptr;
    wxListCtrl* listCtrl_ = nullptr;
    wxButton* homeButton_ = nullptr;
    wxButton* sortButton_ = nullptr;
    wxStaticText* statusText_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_DIR_PATH_BOX = wxID_HIGHEST + 300,
    ID_SSH_DIR_LIST,
    ID_SSH_DIR_HOME_BUTTON,
    ID_SSH_DIR_SORT_BUTTON
};
