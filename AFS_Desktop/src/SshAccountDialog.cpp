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
#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountDialog, wxDialog)
    EVT_UPDATE_UI(wxID_OK, SshAccountDialog::on_update_ok_button)
END_EVENT_TABLE()

SshAccountDialog::SshAccountDialog(SshAccount* ssh_account, wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(400, 10)));

    wxBoxSizer* name_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* name_label = new wxStaticText(this, wxID_STATIC, utilwx::field_label("name"));
    name_label->SetMinSize(wxSize(FromDIP(80), name_label->GetMinSize().y));
    name_sizer->Add(name_label, 0, wxALIGN_CENTER_VERTICAL);
    name_box_ = new wxTextCtrl(this, ID_SSH_NAME_BOX);
    name_sizer->Add(name_box_, 1);
    main_sizer->Add(name_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);

    wxBoxSizer* host_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* host_label = new wxStaticText(this, wxID_STATIC, utilwx::field_label("host"));
    host_label->SetMinSize(wxSize(FromDIP(80), host_label->GetMinSize().y));
    host_sizer->Add(host_label, 0, wxALIGN_CENTER_VERTICAL);
    host_box_ = new wxTextCtrl(this, ID_SSH_HOST_BOX);
    host_sizer->Add(host_box_, 1);
    main_sizer->Add(host_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* user_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* user_label = new wxStaticText(this, wxID_STATIC, utilwx::field_label("user"));
    user_label->SetMinSize(wxSize(FromDIP(80), user_label->GetMinSize().y));
    user_sizer->Add(user_label, 0, wxALIGN_CENTER_VERTICAL);
    user_box_ = new wxTextCtrl(this, ID_SSH_USER_BOX);
    user_sizer->Add(user_box_, 1);
    main_sizer->Add(user_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* password_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* password_label = new wxStaticText(this, wxID_STATIC, utilwx::field_label("password"));
    password_label->SetMinSize(wxSize(FromDIP(80), password_label->GetMinSize().y));
    password_sizer->Add(password_label, 0, wxALIGN_CENTER_VERTICAL);
    password_box_ = new wxTextCtrl(this, ID_SSH_PASSWORD_BOX, wxEmptyString, 
        wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    password_sizer->Add(password_box_, 1);
    main_sizer->Add(password_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(400, 180)));
    Fit();
    Centre();

    if (ssh_account != nullptr)
        set_data(*ssh_account);
}

SshAccountDialog::~SshAccountDialog()
{}

static wxString password_from_account(const std::string& stored)  // [SshAccountDialog.md#password_from_account]
{
    const wxString password = wxString::FromUTF8(stored);
    return password.IsEmpty() && !stored.empty() ? wxString::FromUTF8(stored) : password;
}

void SshAccountDialog::set_data(const SshAccount& account)
{
    name_ = wxString::FromUTF8(account.name());  // [SshAccountDialog.md#utf8-fields]
    user_ = wxString::FromUTF8(account.user());
    host_ = wxString::FromUTF8(account.host());
    password_ = password_from_account(account.password());
}

void SshAccountDialog::save_data(SshAccount& account)
{
    account.set_name(std::string(name_.ToUTF8()));  // [SshAccountDialog.md#utf8-on-the-wire]
    account.set_user(std::string(user_.ToUTF8()));
    account.set_host(std::string(host_.ToUTF8()));
    account.set_password(std::string(password_.ToUTF8()));
}

bool SshAccountDialog::TransferDataToWindow()
{
    name_box_->SetValue(name_);
    user_box_->SetValue(user_);
    host_box_->SetValue(host_);
    password_box_->SetValue(password_);
    return true;
}

bool SshAccountDialog::TransferDataFromWindow()
{
    name_ = name_box_->GetValue();
    user_ = user_box_->GetValue();
    host_ = host_box_->GetValue();
    password_ = password_box_->GetValue();
    return true;
}

std::optional<wxString> validate_ssh_account_name(const wxString& name)
{
    if (name.length() < 2)
        return wxString(translation::translate("ssh_name_too_short"));
    if (name.Contains(":"))  // [SshAccountDialog.md#colon-first]
        return wxString(translation::translate("ssh_name_colon"));
    for (const wxChar c : name) {
        if (!wxIsalnum(c) && c != '_' && c != '-')
            return wxString(translation::translate("ssh_name_characters"));
    }
    return std::nullopt;
}

bool SshAccountDialog::Validate()
{
    wxString name = name_box_->GetValue();

    if (auto err = validate_ssh_account_name(name)) {
        wxMessageBox(*err, translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        name_box_->SetFocus();
        return false;
    }

    if (name != name_ && Config::instance().find_ssh_account_by_name(std::string(name.ToUTF8())) != std::nullopt) {  // [SshAccountDialog.md#name-compared-against-original]
        wxMessageBox(translation::translate("ssh_account_exists", {name.ToStdWstring()}), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        name_box_->SetFocus();
        return false;
    }

    if (host_box_->GetValue().IsEmpty()) {
        wxMessageBox(translation::translate("host_required"), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        host_box_->SetFocus();
        return false;
    }
    if (user_box_->GetValue().IsEmpty()) {
        wxMessageBox(translation::translate("user_required"), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        user_box_->SetFocus();
        return false;
    }
    return true;
}

void SshAccountDialog::on_update_ok_button(wxUpdateUIEvent& event)
{
    event.Enable(!host_box_->GetValue().IsEmpty() && !user_box_->GetValue().IsEmpty());
}
