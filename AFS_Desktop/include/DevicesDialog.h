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

class DevicesDialog : public wxDialog {  // [DevicesDialog.md#DevicesDialog]
public:
    DevicesDialog(wxWindow* parent, wxWindowID id, const wxString& title);
    bool changed() const { return changed_; }
private:
    void populate_list();
    void update_buttons();
    long selected_index() const;
    void on_connect(wxCommandEvent& event);
    void on_unpair(wxCommandEvent& event);
    void on_setup(wxCommandEvent& event);  // [DevicesDialog.md#setup]
    void on_list_select(wxListEvent& event);
    void on_list_deselect(wxListEvent& event);
    void on_end_label_edit(wxListEvent& event);
    void on_close_button(wxCommandEvent& event);

    wxListCtrl* list_ctrl_ = nullptr;
    wxButton* unpair_button_ = nullptr;
    bool changed_ = false;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_DEVICES_LIST = wxID_HIGHEST + 310,
    ID_DEVICES_CONNECT,
    ID_DEVICES_UNPAIR,
    ID_DEVICES_SETUP
};
