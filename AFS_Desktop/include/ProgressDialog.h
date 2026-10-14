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

#include <cstdint>
#include <mutex>

namespace AFS_Core { class SyncControler; }

using namespace AFS_Core;

class ProgressDialog: public wxDialog {
    bool closed_ = false;
    bool can_be_closed_ = false;
    SyncControler& sync_controler_;
    mutable std::mutex mutex_;
public:
    ProgressDialog(wxWindow* parent,
        wxWindowID id,
        const wxString& title,
        SyncControler&,
        uint32_t n_files,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~ProgressDialog();

    void OnCloseWindow(wxCloseEvent& event);
    bool closed() const { return closed_; }
    bool can_be_closed() const { return can_be_closed_; }
    void can_be_closed(bool);
private:
    void on_cancel(wxCommandEvent& event);
    void on_timer_update(wxTimerEvent&);
    void on_update_cancel_button(wxUpdateUIEvent& event);
    uint32_t file_steps_done() const;  // [ProgressDialog.md#file-progress]

    wxGauge* gauge_ = nullptr;
    wxStaticText* current_file_ = nullptr;
    wxTimer timer_;
    uint32_t range_ = 0;
    uint32_t files_done_ = 0;
    uint64_t file_size_ = 0;
    DECLARE_EVENT_TABLE()
};
