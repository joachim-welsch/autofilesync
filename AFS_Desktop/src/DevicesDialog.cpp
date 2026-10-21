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
#include "DevicesDialog.h"
#include "PairDeviceDialog.h"
#include "PhoneTemplatesDialog.h"
#include "Config.h"
#include "DeviceLink.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/datetime.h>

using namespace AFS_Core;
using translation::translate;

BEGIN_EVENT_TABLE(DevicesDialog, wxDialog)
    EVT_BUTTON(ID_DEVICES_CONNECT, DevicesDialog::on_connect)
    EVT_BUTTON(ID_DEVICES_UNPAIR, DevicesDialog::on_unpair)
    EVT_BUTTON(ID_DEVICES_SETUP, DevicesDialog::on_setup)
    EVT_BUTTON(wxID_CLOSE, DevicesDialog::on_close_button)
    EVT_LIST_ITEM_SELECTED(ID_DEVICES_LIST, DevicesDialog::on_list_select)
    EVT_LIST_ITEM_DESELECTED(ID_DEVICES_LIST, DevicesDialog::on_list_deselect)
    EVT_LIST_END_LABEL_EDIT(ID_DEVICES_LIST, DevicesDialog::on_end_label_edit)
END_EVENT_TABLE()

namespace {

wxString status_of(const Device& device)  // [DevicesDialog.md#status]
{
    const auto presence = DeviceLink::instance().presence(device.id);
    if (!presence)
        return translate("device_never_seen");
    const wxString when = wxDateTime(presence->last_seen).Format("%x %X");
    return translate("device_seen", {when.ToStdWstring(), wxString::FromUTF8(presence->address).ToStdWstring()});
}

}

DevicesDialog::DevicesDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    list_ctrl_ = new wxListCtrl(this, ID_DEVICES_LIST, wxDefaultPosition, FromDIP(wxSize(500, 200)), wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL);
    list_ctrl_->InsertColumn(0, translate("name"), wxLIST_FORMAT_LEFT, FromDIP(180));
    list_ctrl_->InsertColumn(1, translate("column_status"), wxLIST_FORMAT_LEFT, FromDIP(300));
    main_sizer->Add(list_ctrl_, 1, wxEXPAND | wxALL, 10);
    wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
    button_sizer->Add(new wxButton(this, ID_DEVICES_CONNECT, translate("menu_connect_phone")), 0, wxRIGHT, 5);
    unpair_button_ = new wxButton(this, ID_DEVICES_UNPAIR, translate("button_unpair"));
    button_sizer->Add(unpair_button_, 0, wxRIGHT, 5);
    button_sizer->Add(new wxButton(this, ID_DEVICES_SETUP, translate("button_setup_sync")), 0, wxRIGHT, 5);
    button_sizer->AddStretchSpacer();
    button_sizer->Add(CreateStdDialogButtonSizer(wxCLOSE), 0);
    main_sizer->Add(button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetEscapeId(wxID_CLOSE);
    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(550, 300)));
    Fit();
    Centre();
    populate_list();
    update_buttons();
}

void DevicesDialog::populate_list()
{
    list_ctrl_->DeleteAllItems();
    long index = 0;
    for (const auto& device : Config::instance().devices_snapshot()) {
        const long item = list_ctrl_->InsertItem(index++, wxString::FromUTF8(device.name));
        list_ctrl_->SetItem(item, 1, status_of(device));
    }
}

void DevicesDialog::update_buttons()
{
    unpair_button_->Enable(selected_index() != -1);
}

long DevicesDialog::selected_index() const
{
    return list_ctrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void DevicesDialog::on_connect(wxCommandEvent&)
{
    PairDeviceDialog dlg(this, wxID_ANY, txt::connect_phone());
    dlg.ShowModal();
    if (dlg.paired()) {
        changed_ = true;
        populate_list();
    }
}

void DevicesDialog::on_unpair(wxCommandEvent&)  // [DevicesDialog.md#unpair]
{
    const long index = selected_index();
    const Devices devices = Config::instance().devices_snapshot();
    if (index < 0 || static_cast<size_t>(index) >= devices.size())
        return;
    const Device& device = devices[static_cast<size_t>(index)];
    const wxString question = translate("confirm_unpair_device", {wxString::FromUTF8(device.name).ToStdWstring()});
    if (!utilwx::confirm(this, question, translate("confirm_delete_title"), translate("button_unpair")))
        return;
    Config::instance().remove_device(device.id);
    changed_ = true;
    populate_list();
    update_buttons();
}

void DevicesDialog::on_setup(wxCommandEvent&)  // [DevicesDialog.md#setup]
{
    const long index = selected_index();
    const Devices devices = Config::instance().devices_snapshot();
    const size_t which = index < 0 ? 0 : static_cast<size_t>(index);
    if (which >= devices.size()) {
        wxMessageBox(translate("no_devices_paired"), translate("info"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    PhoneTemplatesDialog dlg(this, wxID_ANY, txt::phone_templates(), devices[which]);
    dlg.ShowModal();
    if (dlg.created())
        changed_ = true;
}

void DevicesDialog::on_list_select(wxListEvent&)
{
    update_buttons();
}

void DevicesDialog::on_list_deselect(wxListEvent&)
{
    update_buttons();
}

void DevicesDialog::on_end_label_edit(wxListEvent& event)  // [DevicesDialog.md#rename]
{
    if (event.IsEditCancelled())
        return;
    const Devices devices = Config::instance().devices_snapshot();
    const long index = event.GetIndex();
    if (index < 0 || static_cast<size_t>(index) >= devices.size())
        return;
    const wxString name = event.GetLabel().Strip(wxString::both);
    if (name.empty()) {
        wxMessageBox(translate("device_name_empty"), translate("validation_error"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }
    Device device = devices[static_cast<size_t>(index)];
    device.name = std::string(name.ToUTF8());
    Config::instance().update_device(device);
    changed_ = true;
}

void DevicesDialog::on_close_button(wxCommandEvent&)
{
    EndModal(wxID_CLOSE);
}
