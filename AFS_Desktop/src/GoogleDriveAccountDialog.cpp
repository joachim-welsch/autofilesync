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
#include "GoogleDriveAccountDialog.h"
#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"
#include "Translation.h"
#include "Util.h"
#include "utilwx.h"

using namespace AFS_Core;

namespace {
constexpr int LABEL_WIDTH = 80;
constexpr int DIALOG_WIDTH = 520;
constexpr int TEXT_WIDTH = 500;  // [GoogleDriveAccountDialog.md#explanation]
constexpr int POLL_MILLISECONDS = 200;  // [GoogleDriveAccountDialog.md#polling]

GoogleDriveAccountDialog::BrowserOpener& browser_opener()
{
    static GoogleDriveAccountDialog::BrowserOpener opener = [](const wxString& url) { wxLaunchDefaultBrowser(url); };
    return opener;
}
}

void GoogleDriveAccountDialog::open_browser_with(BrowserOpener opener)
{
    browser_opener() = std::move(opener);
}

GoogleDriveAccountDialog::GoogleDriveAccountDialog(const SshAccount& account, wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title), original_name_(wxString::FromUTF8(account.name())), refresh_token_(account.password()), poll_timer_(this)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(DIALOG_WIDTH, 10)));
    wxBoxSizer* name_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* name_label = new wxStaticText(this, wxID_STATIC, utilwx::field_label("name"));
    name_label->SetMinSize(wxSize(FromDIP(LABEL_WIDTH), name_label->GetMinSize().y));
    name_sizer->Add(name_label, 0, wxALIGN_CENTER_VERTICAL);
    name_box_ = new wxTextCtrl(this, ID_GOOGLE_NAME_BOX, original_name_);
    name_sizer->Add(name_box_, 1);
    main_sizer->Add(name_sizer, 0, wxEXPAND | wxALL, 5);
    wxStaticText* explanation = new wxStaticText(this, wxID_STATIC, translation::translate("google_login_explanation"));  // [GoogleDriveAccountDialog.md#explanation]
    explanation->Wrap(FromDIP(TEXT_WIDTH));
    main_sizer->Add(explanation, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    login_button_ = new wxButton(this, ID_GOOGLE_LOGIN_BUTTON, translation::translate("google_login_button"));
    main_sizer->Add(login_button_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    status_ = new wxStaticText(this, ID_GOOGLE_STATUS, translation::translate(refresh_token_.empty() ? "google_login_status_none" : "google_login_status_done"));
    main_sizer->Add(status_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);
    SetSizerAndFit(main_sizer);
    Centre();
    Bind(wxEVT_BUTTON, &GoogleDriveAccountDialog::on_login, this, ID_GOOGLE_LOGIN_BUTTON);
    Bind(wxEVT_TIMER, &GoogleDriveAccountDialog::on_poll, this);
    Bind(wxEVT_UPDATE_UI, &GoogleDriveAccountDialog::on_update_ok_button, this, wxID_OK);
}

void GoogleDriveAccountDialog::show_status(const wxString& text)
{
    status_->SetLabel(text);
    status_->Wrap(FromDIP(TEXT_WIDTH));
    Layout();
    Fit();
}

void GoogleDriveAccountDialog::on_login(wxCommandEvent&)
{
    try {
        login_ = std::make_unique<google_auth::Login>();
        browser_opener()(wxString::FromUTF8(login_->browser_url()));
        show_status(translation::translate("google_login_status_waiting"));
        poll_timer_.Start(POLL_MILLISECONDS);
    } catch (const std::exception& e) {
        show_status(translation::translate("error_with_reason", {util::string_to_wstring(e.what())}));
    }
}

void GoogleDriveAccountDialog::on_poll(wxTimerEvent&)  // [GoogleDriveAccountDialog.md#polling]
{
    if (!login_ || !login_->finished(std::chrono::milliseconds(0)))
        return;
    poll_timer_.Stop();
    try {
        wxBusyCursor wait;
        refresh_token_ = login_->refresh_token();
        show_status(translation::translate("google_login_status_done"));
    } catch (const std::exception& e) {
        show_status(translation::translate("error_with_reason", {util::string_to_wstring(e.what())}));
    }
    login_.reset();
    Raise();
}

void GoogleDriveAccountDialog::save_data(SshAccount& account) const
{
    account.set_name(std::string(name_.ToUTF8()));
    account.set_google_drive(true);
    account.set_url(google_auth::settings().api_url);
    account.set_password(refresh_token_);
}

bool GoogleDriveAccountDialog::TransferDataFromWindow()
{
    name_ = name_box_->GetValue();
    return true;
}

bool GoogleDriveAccountDialog::Validate()  // [SshAccountDialog.md#validate_ssh_account_name]
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

void GoogleDriveAccountDialog::on_update_ok_button(wxUpdateUIEvent& event)
{
    event.Enable(!refresh_token_.empty() && !name_box_->GetValue().IsEmpty());
}
