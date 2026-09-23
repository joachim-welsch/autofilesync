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
#include "SshAccountsDialog.h"
#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"
#include "TextResources.h"
#include "Translation.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountsDialog, wxDialog)
    EVT_BUTTON(ID_SSHACCOUNTS_ADD, SshAccountsDialog::onAdd)
    EVT_BUTTON(ID_SSHACCOUNTS_EDIT, SshAccountsDialog::onEdit)
    EVT_BUTTON(ID_SSHACCOUNTS_DELETE, SshAccountsDialog::onDelete)
    EVT_LIST_ITEM_SELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onListSelect)
    EVT_LIST_ITEM_DESELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onListDeselect)
    EVT_LIST_END_LABEL_EDIT(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onEndLabelEdit)
END_EVENT_TABLE()

SshAccountsDialog::SshAccountsDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    listCtrl_ = new wxListCtrl(this, ID_SSHACCOUNTS_LIST, wxDefaultPosition, 
        wxSize(500, 200), wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL);
    
    listCtrl_->InsertColumn(0, translation::translate("name"), wxLIST_FORMAT_LEFT, 120);
    listCtrl_->InsertColumn(1, translation::translate("host"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(2, translation::translate("user"), wxLIST_FORMAT_LEFT, 100);
    listCtrl_->InsertColumn(3, translation::translate("password"), wxLIST_FORMAT_LEFT, 100);
    
    mainSizer->Add(listCtrl_, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    addButton_ = new wxButton(this, ID_SSHACCOUNTS_ADD, translation::translate("button_add"));
    editButton_ = new wxButton(this, ID_SSHACCOUNTS_EDIT, translation::translate("button_edit"));
    deleteButton_ = new wxButton(this, ID_SSHACCOUNTS_DELETE, translation::translate("button_delete"));
    
    buttonSizer->Add(addButton_, 0, wxRIGHT, 5);
    buttonSizer->Add(editButton_, 0, wxRIGHT, 5);
    buttonSizer->Add(deleteButton_, 0, wxRIGHT, 5);
    buttonSizer->AddStretchSpacer();
    
    wxButton* okButton = new wxButton(this, wxID_OK, translation::translate("button_ok"));
    wxButton* cancelButton = new wxButton(this, wxID_CANCEL, translation::translate("button_cancel"));
    buttonSizer->Add(okButton, 0, wxRIGHT, 5);
    buttonSizer->Add(cancelButton, 0);
    
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(mainSizer);
    SetMinSize(wxSize(550, 300));
    Fit();
    Centre();
    
    listCtrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountsDialog::onListDClick, this);
    
    populateList();
    updateButtons();
}

SshAccountsDialog::~SshAccountsDialog()
{}

void SshAccountsDialog::populateList()
{
    listCtrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().ssh_accounts_snapshot();
    long index = 0;
    for (const auto& account : accounts) {
        long item = listCtrl_->InsertItem(index, wxString::FromUTF8(account.name()));  // [SshAccountsDialog.md#utf8-list]
        listCtrl_->SetItem(item, 1, wxString::FromUTF8(account.host()));
        listCtrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        listCtrl_->SetItem(item, 3, wxString(account.password().empty() ? "" : "********"));
        index++;
    }
}

void SshAccountsDialog::updateButtons()
{
    bool hasSelection = getSelectedIndex() != -1;
    editButton_->Enable(hasSelection);
    deleteButton_->Enable(hasSelection);
}

long SshAccountsDialog::getSelectedIndex() const
{
    return listCtrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void SshAccountsDialog::onAdd(wxCommandEvent&)
{
    SshAccount newAccount;
    SshAccountDialog dlg(&newAccount, this, wxID_ANY, txt::add_ssh_connection());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(newAccount);
        Config::instance().add(newAccount);
        populateList();
    }
}

void SshAccountsDialog::onEdit(wxCommandEvent&)
{
    editSelectedItem();
}

void SshAccountsDialog::onDelete(wxCommandEvent&)
{
    long sel = getSelectedIndex();
    if (sel == -1)
        return;

    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    wxString message = translation::translate("confirm_delete_ssh_account", {wxString::FromUTF8(account->name()).ToStdWstring()});
    if (Config::instance().ssh_account_referenced_by_any_item(account->name()))  // [SshAccountsDialog.md#deletion-warns]
        message += L"\n\n" + translation::translate("ssh_account_in_use_warning");

    int answer = wxMessageBox(message, translation::translate("confirm_delete_title"), wxYES_NO | wxICON_QUESTION, this);

    if (answer == wxYES) {
        Config::instance().remove_ssh_account(static_cast<size_t>(sel));
        populateList();
        updateButtons();
    }
}

void SshAccountsDialog::onListDClick(wxMouseEvent&)
{
    editSelectedItem();
}

void SshAccountsDialog::onListSelect(wxListEvent&)
{
    updateButtons();
}

void SshAccountsDialog::onListDeselect(wxListEvent&)
{
    updateButtons();
}

void SshAccountsDialog::onEndLabelEdit(wxListEvent& event)
{
    if (event.IsEditCancelled())
        return;

    long sel = event.GetIndex();
    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    if (auto err = validateSshAccountName(event.GetLabel())) {
        wxMessageBox(*err, translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }

    const std::string oldName = account->name();
    const std::string newName = std::string(event.GetLabel().ToUTF8());
    if (newName != oldName && Config::instance().find_ssh_account_by_name(newName) != std::nullopt) {
        wxMessageBox(translation::translate("ssh_account_exists", {event.GetLabel().ToStdWstring()}),
            translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }
    account->set_name(newName);
    Config::instance().update_ssh_account(static_cast<size_t>(sel), *account);
    Config::instance().update_ssh_account_references(oldName, *account);  // [SshAccountsDialog.md#account-references-updated]
}

void SshAccountsDialog::editSelectedItem()
{
    long sel = getSelectedIndex();
    if (sel == -1)
        return;

    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    const std::string oldName = account->name();
    SshAccountDialog dlg(&account.value(), this, wxID_ANY, txt::edit_ssh_connection());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(account.value());
        Config::instance().update_ssh_account(static_cast<size_t>(sel), account.value());
        Config::instance().update_ssh_account_references(oldName, account.value());  // [SshAccountsDialog.md#account-references-updated]
        populateList();
    }
}
