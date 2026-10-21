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

#include <wx/filepicker.h>
#include "Device.h"
#include "SyncItem.h"
#include <optional>

enum class PhoneTemplate { Photos, Folder, Music, None };  // [PhoneTemplatesDialog.md#templates]

std::optional<AFS_Core::SyncItem> phone_template_item(PhoneTemplate which, const AFS_Core::Device& device, const wxString& pc_folder, const wxString& phone_folder);  // [PhoneTemplatesDialog.md#phone_template_item]
wxString default_pc_folder(PhoneTemplate which, const AFS_Core::Device& device);

class PhoneTemplatesDialog : public wxDialog {  // [PhoneTemplatesDialog.md#PhoneTemplatesDialog]
public:
    PhoneTemplatesDialog(wxWindow* parent, wxWindowID id, const wxString& title, const AFS_Core::Device& device);
    bool created() const { return created_; }
private:
    PhoneTemplate chosen() const;
    void on_template(wxCommandEvent& event);
    void on_ok(wxCommandEvent& event);

    AFS_Core::Device device_;
    wxRadioBox* templates_ = nullptr;
    wxDirPickerCtrl* pc_folder_ = nullptr;
    wxComboBox* phone_folder_ = nullptr;
    bool created_ = false;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_TEMPLATES_RADIO = wxID_HIGHEST + 330,
    ID_TEMPLATES_PC_FOLDER,
    ID_TEMPLATES_PHONE_FOLDER
};
