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
#include "PhoneDirDialog.h"
#include "Config.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;
using translation::translate;

std::optional<Device> device_of_path(const wxString& path)  // [PhoneDirDialog.md#device_of_path]
{
    const int colon = path.Find(':');
    if (colon <= 0)
        return std::nullopt;
    return Config::instance().find_device(std::string(path.Left(colon).ToUTF8()));
}

wxString display_path(const wxString& path)  // [PhoneDirDialog.md#display_path]
{
    const auto device = device_of_path(path);
    if (!device)
        return path;
    return L"\U0001F4F1 " + wxString::FromUTF8(device->name) + path.Mid(path.Find(':'));
}

BEGIN_EVENT_TABLE(PhoneDirDialog, wxDialog)
    EVT_BUTTON(wxID_OK, PhoneDirDialog::on_ok)
END_EVENT_TABLE()

PhoneDirDialog::PhoneDirDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxString& current_path)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE), devices_(Config::instance().devices_snapshot())
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(8));
    grid->AddGrowableCol(1);
    grid->Add(new wxStaticText(this, wxID_STATIC, utilwx::field_label("phone_dir_phone")), 0, wxALIGN_CENTER_VERTICAL);
    phone_choice_ = new wxChoice(this, ID_PHONEDIR_PHONE);
    for (const auto& device : devices_)
        phone_choice_->Append(wxString::FromUTF8(device.name));
    grid->Add(phone_choice_, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_STATIC, utilwx::field_label("phone_dir_folder")), 0, wxALIGN_CENTER_VERTICAL);
    folder_box_ = new wxComboBox(this, ID_PHONEDIR_FOLDER, PHONE_FOLDERS[0]);
    for (const wchar_t* folder : PHONE_FOLDERS)
        folder_box_->Append(folder);
    grid->Add(folder_box_, 1, wxEXPAND);
    main_sizer->Add(grid, 0, wxEXPAND | wxALL, 10);
    main_sizer->Add(new wxStaticText(this, wxID_STATIC, translate("phone_dir_hint")), 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    main_sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(420, -1)));
    if (const auto current = device_of_path(current_path)) {  // [PhoneDirDialog.md#current-path]
        for (size_t i = 0; i < devices_.size(); ++i)
            if (devices_[i].id == current->id)
                phone_choice_->SetSelection(static_cast<int>(i));
        folder_box_->SetValue(current_path.Mid(current_path.Find(':') + 1));
    } else if (!devices_.empty())
        phone_choice_->SetSelection(0);
    Fit();
    Centre();
}

wxString PhoneDirDialog::path() const
{
    const int selected = phone_choice_->GetSelection();
    if (selected < 0 || static_cast<size_t>(selected) >= devices_.size())
        return wxEmptyString;
    wxString folder = folder_box_->GetValue().Strip(wxString::both);
    if (!folder.StartsWith("/"))
        folder = "/" + folder;
    return wxString::FromUTF8(devices_[static_cast<size_t>(selected)].id) + ":" + folder;
}

void PhoneDirDialog::on_ok(wxCommandEvent& event)
{
    if (path().empty()) {
        wxMessageBox(translate("no_devices_paired"), translate("validation_error"), wxOK | wxICON_WARNING, this);
        return;
    }
    event.Skip();
}
