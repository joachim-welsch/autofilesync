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
#include "SshAccountSelectDialog.h"
#include "utilwx.h"
#include "Config.h"
#include "Translation.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountSelectDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_SSH_SELECT_LIST, SshAccountSelectDialog::on_list_select)
END_EVENT_TABLE()

SshAccountSelectDialog::SshAccountSelectDialog(wxWindow* parent,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxDialog(parent, wxID_ANY, title, pos, size, style)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* info_text = new wxStaticText(this, wxID_STATIC,
        translation::translate("select_ssh_connection_hint"));
    main_sizer->Add(info_text, 0, wxEXPAND | wxALL, 10);

    list_ctrl_ = new wxListCtrl(this, ID_SSH_SELECT_LIST, wxDefaultPosition,
        FromDIP(wxSize(400, 200)), wxLC_REPORT | wxLC_SINGLE_SEL);
    
    list_ctrl_->InsertColumn(0, translation::translate("name"), wxLIST_FORMAT_LEFT, 150);
    list_ctrl_->InsertColumn(1, translation::translate("host"), wxLIST_FORMAT_LEFT, 150);
    list_ctrl_->InsertColumn(2, translation::translate("user"), wxLIST_FORMAT_LEFT, 100);
    
    main_sizer->Add(list_ctrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    list_ctrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountSelectDialog::on_list_double_click, this);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(450, 300)));
    Fit();
    Centre();

    populate_list();
    update_buttons();
}

SshAccountSelectDialog::~SshAccountSelectDialog()
{}

void SshAccountSelectDialog::populate_list()
{
    list_ctrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().ssh_accounts_snapshot();
    long index = 0;
    for (const auto& account : accounts) {
        long item = list_ctrl_->InsertItem(index, wxString::FromUTF8(account.name()));  // [SshAccountsDialog.md#utf8-list]
        list_ctrl_->SetItem(item, 1, utilwx::place_of(account));  // [SshAccountsDialog.md#webdav-accounts]
        list_ctrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        index++;
    }
}

void SshAccountSelectDialog::on_list_double_click(wxMouseEvent&)
{
    if (selected_index_ >= 0) {
        EndModal(wxID_OK);
    }
}

void SshAccountSelectDialog::on_list_select(wxListEvent& event)
{
    selected_index_ = event.GetIndex();
    update_buttons();
}

void SshAccountSelectDialog::update_buttons()
{
    wxButton* ok_button = dynamic_cast<wxButton*>(FindWindow(wxID_OK));
    if (ok_button) {
        ok_button->Enable(selected_index_ >= 0);
    }
}

std::optional<SshAccount> SshAccountSelectDialog::get_selected_account() const
{
    if (selected_index_ < 0) {
        return std::nullopt;
    }
    return Config::instance().ssh_account_at(static_cast<size_t>(selected_index_));
}
