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
#include "WebDavAccountDialog.h"
#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"
#include "Translation.h"
#include "Util.h"
#include "WebDav.h"
#include "utilwx.h"

using namespace AFS_Core;

namespace {
constexpr int LABEL_WIDTH = 80;
constexpr int DIALOG_WIDTH = 520;
constexpr int HINT_WIDTH = 500;  // [WebDavAccountDialog.md#address-hint]
}

WebDavAccountDialog::WebDavAccountDialog(const SshAccount& account, wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title), original_name_(wxString::FromUTF8(account.name()))
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(DIALOG_WIDTH, 10)));
    name_box_ = add_field(main_sizer, ID_WEBDAV_NAME_BOX, "name", original_name_);
    url_box_ = add_field(main_sizer, ID_WEBDAV_URL_BOX, "webdav_address", wxString::FromUTF8(account.url()));
    wxStaticText* hint = new wxStaticText(this, wxID_STATIC, translation::translate("webdav_address_hint"));  // [WebDavAccountDialog.md#address-hint]
    hint->Wrap(FromDIP(HINT_WIDTH));
    main_sizer->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    user_box_ = add_field(main_sizer, ID_WEBDAV_USER_BOX, "user", wxString::FromUTF8(account.user()));
    password_box_ = add_field(main_sizer, ID_WEBDAV_PASSWORD_BOX, "password", wxString::FromUTF8(account.password()), wxTE_PASSWORD);
    main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);
    SetSizerAndFit(main_sizer);
    Centre();
    Bind(wxEVT_UPDATE_UI, &WebDavAccountDialog::on_update_ok_button, this, wxID_OK);
}

wxTextCtrl* WebDavAccountDialog::add_field(wxSizer* sizer, wxWindowID id, const char* label_msgid, const wxString& value, long style)
{
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* label = new wxStaticText(this, wxID_STATIC, utilwx::field_label(label_msgid));
    label->SetMinSize(wxSize(FromDIP(LABEL_WIDTH), label->GetMinSize().y));
    row->Add(label, 0, wxALIGN_CENTER_VERTICAL);
    wxTextCtrl* box = new wxTextCtrl(this, id, value, wxDefaultPosition, wxDefaultSize, style);
    row->Add(box, 1);
    sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    return box;
}

void WebDavAccountDialog::save_data(SshAccount& account) const
{
    account.set_name(std::string(name_.ToUTF8()));
    account.set_url(std::string(url_.ToUTF8()));
    account.set_user(std::string(user_.ToUTF8()));
    account.set_password(std::string(password_.ToUTF8()));
}

bool WebDavAccountDialog::TransferDataFromWindow()
{
    name_ = name_box_->GetValue();
    url_ = url_box_->GetValue().Strip(wxString::both);
    user_ = user_box_->GetValue();
    password_ = password_box_->GetValue();
    return true;
}

bool WebDavAccountDialog::name_valid()  // [SshAccountDialog.md#validate_ssh_account_name]
{
    const wxString name = name_box_->GetValue();
    const bool taken = name != original_name_ && Config::instance().find_ssh_account_by_name(std::string(name.ToUTF8())).has_value();
    const std::optional<wxString> problem = taken ? wxString(translation::translate("ssh_account_exists", {name.ToStdWstring()})) : validate_ssh_account_name(name);
    if (!problem)
        return true;
    wxMessageBox(*problem, translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
    name_box_->SetFocus();
    return false;
}

bool WebDavAccountDialog::address_valid()  // [WebDav.md#https-only]
{
    if (webdav::address_allowed(std::string(url_box_->GetValue().Strip(wxString::both).ToUTF8())))
        return true;
    wxMessageBox(translation::translate("webdav_address_needs_https"), translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
    url_box_->SetFocus();
    return false;
}

bool WebDavAccountDialog::server_answers()  // [WebDavAccountDialog.md#connection-check]
{
    SshAccount probe;
    probe.set_url(std::string(url_box_->GetValue().Strip(wxString::both).ToUTF8()));
    probe.set_user(std::string(user_box_->GetValue().ToUTF8()));
    probe.set_password(std::string(password_box_->GetValue().ToUTF8()));
    std::string problem;
    try {
        wxBusyCursor wait;
        if (webdav::stat(probe, "/"))
            return true;
        problem = util::wstring_to_string(translation::translate("webdav_folder_not_found"));
    } catch (const std::exception& e) {
        problem = e.what();
    }
    return utilwx::confirm(this, translation::translate("webdav_check_failed_save_anyway", {util::string_to_wstring(problem)}),
        translation::translate("webdav_check_caption"), translation::translate("button_save_anyway"));
}

bool WebDavAccountDialog::Validate()
{
    return name_valid() && address_valid() && server_answers();
}

void WebDavAccountDialog::on_update_ok_button(wxUpdateUIEvent& event)
{
    event.Enable(!url_box_->GetValue().IsEmpty() && !user_box_->GetValue().IsEmpty());
}
