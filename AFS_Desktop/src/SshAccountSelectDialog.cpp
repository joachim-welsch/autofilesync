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
#include "Config.h"
#include "Translation.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountSelectDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_SSH_SELECT_LIST, SshAccountSelectDialog::onListSelect)
END_EVENT_TABLE()

SshAccountSelectDialog::SshAccountSelectDialog(wxWindow* parent,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxDialog(parent, wxID_ANY, title, pos, size, style)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* infoText = new wxStaticText(this, wxID_STATIC,
        translation::translate("select_ssh_connection_hint"));
    mainSizer->Add(infoText, 0, wxEXPAND | wxALL, 10);

    listCtrl_ = new wxListCtrl(this, ID_SSH_SELECT_LIST, wxDefaultPosition,
        wxSize(400, 200), wxLC_REPORT | wxLC_SINGLE_SEL);
    
    listCtrl_->InsertColumn(0, translation::translate("name"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(1, translation::translate("host"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(2, translation::translate("user"), wxLIST_FORMAT_LEFT, 100);
    
    mainSizer->Add(listCtrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    listCtrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountSelectDialog::onListDClick, this);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);

    SetSizer(mainSizer);
    SetMinSize(wxSize(450, 300));
    Fit();
    Centre();

    populateList();
    updateButtons();
}

SshAccountSelectDialog::~SshAccountSelectDialog()
{}

void SshAccountSelectDialog::populateList()
{
    listCtrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().ssh_accounts_snapshot();
    long index = 0;
    for (const auto& account : accounts) {
        long item = listCtrl_->InsertItem(index, wxString::FromUTF8(account.name()));  // [SshAccountsDialog.md#utf8-list]
        listCtrl_->SetItem(item, 1, wxString::FromUTF8(account.host()));
        listCtrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        index++;
    }
}

void SshAccountSelectDialog::onListDClick(wxMouseEvent&)
{
    if (selectedIndex_ >= 0) {
        EndModal(wxID_OK);
    }
}

void SshAccountSelectDialog::onListSelect(wxListEvent& event)
{
    selectedIndex_ = event.GetIndex();
    updateButtons();
}

void SshAccountSelectDialog::updateButtons()
{
    wxButton* okButton = dynamic_cast<wxButton*>(FindWindow(wxID_OK));
    if (okButton) {
        okButton->Enable(selectedIndex_ >= 0);
    }
}

std::optional<SshAccount> SshAccountSelectDialog::getSelectedAccount() const
{
    if (selectedIndex_ < 0) {
        return std::nullopt;
    }
    return Config::instance().ssh_account_at(static_cast<size_t>(selectedIndex_));
}
