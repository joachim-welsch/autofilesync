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

class SettingsDialog: public wxDialog {
public:
    SettingsDialog(
        wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SettingsDialog();
private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    void onPickRecycleBinPath(wxCommandEvent&);
    void onDeleteModeChanged(wxCommandEvent&);
    void enableRecycleBinPath();

    wxRadioBox* deleteModeRB_ = nullptr;
    wxTextCtrl* recycleBinPathBox_ = nullptr;
    wxButton*   recycleBinPathBtn_ = nullptr;
    wxCheckBox* startMinimizedCB_ = nullptr;
    wxCheckBox* minimizeInsteadOfCloseCB_ = nullptr;
    wxCheckBox* autoStartCB_ = nullptr;
    wxCheckBox* startSyncAtStartCB_ = nullptr;
    wxCheckBox* messageBoxErrorCB_ = nullptr;
    wxCheckBox* messageBoxWarningCB_ = nullptr;
    wxCheckBox* messageBoxInfoCB_ = nullptr;
    DECLARE_EVENT_TABLE()
};

enum {
    ID_DELETE_MODE_RADIO = wxID_HIGHEST + 1,
    ID_RECYCLEBIN_PATH_BOX,
    ID_RECYCLEBIN_PATH_BUTTON
};

