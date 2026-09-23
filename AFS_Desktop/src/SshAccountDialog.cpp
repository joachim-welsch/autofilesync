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

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountDialog, wxDialog)
    EVT_UPDATE_UI(wxID_OK, SshAccountDialog::onUpdateOkButton)
END_EVENT_TABLE()

SshAccountDialog::SshAccountDialog(SshAccount* sshAccount, wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->AddStretchSpacer()->SetMinSize(400, 10);

    wxBoxSizer* nameSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* nameLabel = new wxStaticText(this, wxID_STATIC, translation::translate("name"));
    nameLabel->SetMinSize(wxSize(80, nameLabel->GetMinSize().y));
    nameSizer->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL);
    nameBox_ = new wxTextCtrl(this, ID_SSH_NAME_BOX);
    nameSizer->Add(nameBox_, 1);
    mainSizer->Add(nameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);

    wxBoxSizer* hostSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* hostLabel = new wxStaticText(this, wxID_STATIC, translation::translate("host"));
    hostLabel->SetMinSize(wxSize(80, hostLabel->GetMinSize().y));
    hostSizer->Add(hostLabel, 0, wxALIGN_CENTER_VERTICAL);
    hostBox_ = new wxTextCtrl(this, ID_SSH_HOST_BOX);
    hostSizer->Add(hostBox_, 1);
    mainSizer->Add(hostSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* userSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* userLabel = new wxStaticText(this, wxID_STATIC, translation::translate("user"));
    userLabel->SetMinSize(wxSize(80, userLabel->GetMinSize().y));
    userSizer->Add(userLabel, 0, wxALIGN_CENTER_VERTICAL);
    userBox_ = new wxTextCtrl(this, ID_SSH_USER_BOX);
    userSizer->Add(userBox_, 1);
    mainSizer->Add(userSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* passwordSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* passwordLabel = new wxStaticText(this, wxID_STATIC, translation::translate("password"));
    passwordLabel->SetMinSize(wxSize(80, passwordLabel->GetMinSize().y));
    passwordSizer->Add(passwordLabel, 0, wxALIGN_CENTER_VERTICAL);
    passwordBox_ = new wxTextCtrl(this, ID_SSH_PASSWORD_BOX, wxEmptyString, 
        wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    passwordSizer->Add(passwordBox_, 1);
    mainSizer->Add(passwordSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(400, 180));
    Fit();
    Centre();

    if (sshAccount != nullptr)
        setData(*sshAccount);
}

SshAccountDialog::~SshAccountDialog()
{}

static wxString passwordFromAccount(const std::string& stored)  // [SshAccountDialog.md#passwordFromAccount]
{
    const wxString password = wxString::FromUTF8(stored);
    return password.IsEmpty() && !stored.empty() ? wxString::FromUTF8(stored) : password;
}

void SshAccountDialog::setData(const SshAccount& account)
{
    name_ = wxString::FromUTF8(account.name());  // [SshAccountDialog.md#utf8-fields]
    user_ = wxString::FromUTF8(account.user());
    host_ = wxString::FromUTF8(account.host());
    password_ = passwordFromAccount(account.password());
}

void SshAccountDialog::saveData(SshAccount& account)
{
    account.set_name(std::string(name_.ToUTF8()));  // [SshAccountDialog.md#utf8-on-the-wire]
    account.set_user(std::string(user_.ToUTF8()));
    account.set_host(std::string(host_.ToUTF8()));
    account.set_password(std::string(password_.ToUTF8()));
}

bool SshAccountDialog::TransferDataToWindow()
{
    nameBox_->SetValue(name_);
    userBox_->SetValue(user_);
    hostBox_->SetValue(host_);
    passwordBox_->SetValue(password_);
    return true;
}

bool SshAccountDialog::TransferDataFromWindow()
{
    name_ = nameBox_->GetValue();
    user_ = userBox_->GetValue();
    host_ = hostBox_->GetValue();
    password_ = passwordBox_->GetValue();
    return true;
}

std::optional<wxString> validateSshAccountName(const wxString& name)
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
    wxString name = nameBox_->GetValue();

    if (auto err = validateSshAccountName(name)) {
        wxMessageBox(*err, translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        nameBox_->SetFocus();
        return false;
    }

    if (name != name_ && Config::instance().find_ssh_account_by_name(std::string(name.ToUTF8())) != std::nullopt) {  // [SshAccountDialog.md#name-compared-against-original]
        wxMessageBox(translation::translate("ssh_account_exists", {name.ToStdWstring()}), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        nameBox_->SetFocus();
        return false;
    }

    if (hostBox_->GetValue().IsEmpty()) {
        wxMessageBox(translation::translate("host_required"), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        hostBox_->SetFocus();
        return false;
    }
    if (userBox_->GetValue().IsEmpty()) {
        wxMessageBox(translation::translate("user_required"), translation::translate("validation_error"),
            wxOK | wxICON_WARNING, this);
        userBox_->SetFocus();
        return false;
    }
    return true;
}

void SshAccountDialog::onUpdateOkButton(wxUpdateUIEvent& event)
{
    event.Enable(!hostBox_->GetValue().IsEmpty() && !userBox_->GetValue().IsEmpty());
}
