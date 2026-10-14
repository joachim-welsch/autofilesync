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
    void on_pick_recycle_bin_path(wxCommandEvent&);
    void on_delete_mode_changed(wxCommandEvent&);
    void enable_recycle_bin_path();
    void add_language_choice(wxSizer* sizer);

    wxChoice*   language_choice_ = nullptr;
    wxRadioBox* delete_mode_radio_ = nullptr;
    wxTextCtrl* recycle_bin_path_box_ = nullptr;
    wxButton*   recycle_bin_path_button_ = nullptr;
    wxCheckBox* start_minimized_check_ = nullptr;
    wxCheckBox* minimize_instead_of_close_check_ = nullptr;
    wxCheckBox* auto_start_check_ = nullptr;
    wxCheckBox* start_sync_at_start_check_ = nullptr;
    wxCheckBox* message_box_error_check_ = nullptr;
    wxCheckBox* message_box_warning_check_ = nullptr;
    wxCheckBox* message_box_info_check_ = nullptr;
    DECLARE_EVENT_TABLE()
};

enum {
    ID_DELETE_MODE_RADIO = wxID_HIGHEST + 1,
    ID_RECYCLEBIN_PATH_BOX,
    ID_RECYCLEBIN_PATH_BUTTON,
    ID_LANGUAGE_CHOICE
};

