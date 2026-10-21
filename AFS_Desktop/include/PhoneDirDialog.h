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
#include <array>
#include <optional>

inline constexpr std::array<const wchar_t*, 7> PHONE_FOLDERS{  // [PhoneDirDialog.md#folders]
    L"/DCIM/Camera", L"/Pictures", L"/Pictures/Screenshots", L"/Download", L"/Documents", L"/Music", L"/Movies"};

std::optional<AFS_Core::Device> device_of_path(const wxString& path);  // [PhoneDirDialog.md#device_of_path]
wxString display_path(const wxString& path);  // [PhoneDirDialog.md#display_path]

class PhoneDirDialog : public wxDialog {  // [PhoneDirDialog.md#PhoneDirDialog]
public:
    PhoneDirDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxString& current_path);
    wxString path() const;  // "<device id>:<folder>"
private:
    void on_ok(wxCommandEvent& event);

    wxChoice* phone_choice_ = nullptr;
    wxComboBox* folder_box_ = nullptr;
    AFS_Core::Devices devices_;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_PHONEDIR_PHONE = wxID_HIGHEST + 320,
    ID_PHONEDIR_FOLDER
};
