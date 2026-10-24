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

#include "Device.h"
#include <filesystem>
#include <optional>

std::filesystem::path device_identity_dir();  // [PairDeviceDialog.md#device_identity_dir]
bool start_device_link();  // [PairDeviceDialog.md#start_device_link]

class PairDeviceDialog : public wxDialog {  // [PairDeviceDialog.md#PairDeviceDialog]
public:
    PairDeviceDialog(wxWindow* parent, wxWindowID id, const wxString& title);
    ~PairDeviceDialog() override;
    const std::optional<AFS_Core::Device>& paired() const { return paired_; }
private:
    void show_offer();
    void on_timer(wxTimerEvent& event);
    void on_close_button(wxCommandEvent& event);

    wxStaticBitmap* code_ = nullptr;
    wxStaticText* steps_ = nullptr;
    wxStaticText* status_ = nullptr;
    wxStaticText* details_ = nullptr;
    wxTimer timer_;
    int ticks_ = 0;
    int closed_ticks_ = 0;
    std::optional<AFS_Core::Device> paired_;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_PAIRDEVICE_TIMER = wxID_HIGHEST + 300
};
