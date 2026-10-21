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
#include "PhoneTemplatesDialog.h"
#include "PhoneDirDialog.h"
#include "Config.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>

using namespace AFS_Core;
using translation::translate;

namespace {

const wchar_t* phone_folder_of(PhoneTemplate which)
{
    switch (which) {
    case PhoneTemplate::Photos: return L"/DCIM/Camera";
    case PhoneTemplate::Music: return L"/Music";
    default: return L"/Documents";
    }
}

wxString unique_name(const wxString& wanted)  // [PhoneTemplatesDialog.md#unique_name]
{
    wxString name = wanted;
    for (int n = 2; Config::instance().find_by_name(name.ToStdWstring()); ++n)
        name = wxString::Format("%s (%d)", wanted, n);
    return name;
}

}

wxString default_pc_folder(PhoneTemplate which, const Device& device)
{
    const wxStandardPaths& paths = wxStandardPaths::Get();
    const wxString name = wxString::FromUTF8(device.name);
    switch (which) {
    case PhoneTemplate::Photos: return wxFileName(paths.GetUserDir(wxStandardPaths::Dir_Pictures), name).GetFullPath();
    case PhoneTemplate::Music: return paths.GetUserDir(wxStandardPaths::Dir_Music);
    case PhoneTemplate::Folder: return wxFileName(paths.GetUserDir(wxStandardPaths::Dir_Documents), name).GetFullPath();
    default: return wxEmptyString;
    }
}

std::optional<SyncItem> phone_template_item(PhoneTemplate which, const Device& device, const wxString& pc_folder, const wxString& phone_folder)  // [PhoneTemplatesDialog.md#phone_template_item]
{
    if (which == PhoneTemplate::None || pc_folder.empty())
        return std::nullopt;
    const wxString phone_path = wxString::FromUTF8(device.id) + ":" + (phone_folder.empty() ? wxString(phone_folder_of(which)) : phone_folder);
    const wxString name = unique_name(translate(which == PhoneTemplate::Photos ? "item_name_photos" : which == PhoneTemplate::Music ? "item_name_music" : "item_name_folder", {wxString::FromUTF8(device.name).ToStdWstring()}));
    const bool to_phone = which == PhoneTemplate::Music;
    SyncItem item(name.ToStdWstring(), SyncItemType::Tree, (to_phone ? pc_folder : phone_path).ToStdWstring(), (to_phone ? phone_path : pc_folder).ToStdWstring(), L"*");
    item.warn_if_unreachable(false);  // a phone that is away is the normal state [PhoneTemplatesDialog.md#templates]
    item.delete_in_target(which == PhoneTemplate::Music);
    item.two_way_sync(which == PhoneTemplate::Folder);
    item.conflict_copies(which == PhoneTemplate::Folder);
    item.track_moves(which == PhoneTemplate::Folder);
    return item;
}

BEGIN_EVENT_TABLE(PhoneTemplatesDialog, wxDialog)
    EVT_RADIOBOX(ID_TEMPLATES_RADIO, PhoneTemplatesDialog::on_template)
    EVT_BUTTON(wxID_OK, PhoneTemplatesDialog::on_ok)
END_EVENT_TABLE()

PhoneTemplatesDialog::PhoneTemplatesDialog(wxWindow* parent, wxWindowID id, const wxString& title, const Device& device)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE), device_(device)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    const wxString choices[] = {translate("template_photos"), translate("template_folder"), translate("template_music"), translate("template_none")};
    templates_ = new wxRadioBox(this, ID_TEMPLATES_RADIO, translate("template_question", {wxString::FromUTF8(device.name).ToStdWstring()}), wxDefaultPosition, wxDefaultSize, 4, choices, 1, wxRA_SPECIFY_COLS);
    main_sizer->Add(templates_, 0, wxEXPAND | wxALL, 10);
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(8));
    grid->AddGrowableCol(1);
    grid->Add(new wxStaticText(this, wxID_STATIC, utilwx::field_label("template_pc_folder")), 0, wxALIGN_CENTER_VERTICAL);
    pc_folder_ = new wxDirPickerCtrl(this, ID_TEMPLATES_PC_FOLDER, default_pc_folder(PhoneTemplate::Photos, device), wxDirSelectorPromptStr, wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL | wxDIRP_DIR_MUST_EXIST);
    grid->Add(pc_folder_, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_STATIC, utilwx::field_label("template_phone_folder")), 0, wxALIGN_CENTER_VERTICAL);
    phone_folder_ = new wxComboBox(this, ID_TEMPLATES_PHONE_FOLDER, phone_folder_of(PhoneTemplate::Photos));
    for (const wchar_t* folder : PHONE_FOLDERS)
        phone_folder_->Append(folder);
    grid->Add(phone_folder_, 1, wxEXPAND);
    main_sizer->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    main_sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(520, -1)));
    Fit();
    Centre();
}

PhoneTemplate PhoneTemplatesDialog::chosen() const
{
    return static_cast<PhoneTemplate>(templates_->GetSelection());
}

void PhoneTemplatesDialog::on_template(wxCommandEvent&)  // [PhoneTemplatesDialog.md#defaults-follow-the-choice]
{
    const PhoneTemplate which = chosen();
    const bool wants_folders = which != PhoneTemplate::None;
    pc_folder_->Enable(wants_folders);
    phone_folder_->Enable(wants_folders);
    if (!wants_folders)
        return;
    pc_folder_->SetPath(default_pc_folder(which, device_));
    phone_folder_->SetValue(phone_folder_of(which));
}

void PhoneTemplatesDialog::on_ok(wxCommandEvent& event)
{
    if (chosen() == PhoneTemplate::None) {
        event.Skip();
        return;
    }
    const wxString pc_folder = pc_folder_->GetPath();
    if (pc_folder.empty()) {
        wxMessageBox(translate("template_pc_folder_missing"), translate("validation_error"), wxOK | wxICON_WARNING, this);
        return;
    }
    wxFileName::Mkdir(pc_folder, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);  // the default folders may not exist yet
    const auto item = phone_template_item(chosen(), device_, pc_folder, phone_folder_->GetValue().Strip(wxString::both));
    if (!item)
        return;
    Config::instance().add(*item);
    created_ = true;
    event.Skip();
}
