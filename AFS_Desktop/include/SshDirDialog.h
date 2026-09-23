/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
