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
#include "WebDavAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountsDialog, wxDialog)
    EVT_BUTTON(ID_SSHACCOUNTS_ADD, SshAccountsDialog::on_add)
    EVT_BUTTON(ID_SSHACCOUNTS_EDIT, SshAccountsDialog::on_edit)
    EVT_BUTTON(ID_SSHACCOUNTS_DELETE, SshAccountsDialog::on_delete)
    EVT_LIST_ITEM_SELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::on_list_select)
    EVT_LIST_ITEM_DESELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::on_list_deselect)
    EVT_LIST_END_LABEL_EDIT(ID_SSHACCOUNTS_LIST, SshAccountsDialog::on_end_label_edit)
END_EVENT_TABLE()

SshAccountsDialog::SshAccountsDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    list_ctrl_ = new wxListCtrl(this, ID_SSHACCOUNTS_LIST, wxDefaultPosition, 
        FromDIP(wxSize(500, 200)), wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL);
    
    list_ctrl_->InsertColumn(0, translation::translate("name"), wxLIST_FORMAT_LEFT, 120);
    list_ctrl_->InsertColumn(1, translation::translate("host"), wxLIST_FORMAT_LEFT, 150);
    list_ctrl_->InsertColumn(2, translation::translate("user"), wxLIST_FORMAT_LEFT, 100);
    list_ctrl_->InsertColumn(3, translation::translate("password"), wxLIST_FORMAT_LEFT, 100);
    
    main_sizer->Add(list_ctrl_, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    add_button_ = new wxButton(this, ID_SSHACCOUNTS_ADD, translation::translate("button_add"));
    edit_button_ = new wxButton(this, ID_SSHACCOUNTS_EDIT, translation::translate("button_edit"));
    delete_button_ = new wxButton(this, ID_SSHACCOUNTS_DELETE, translation::translate("button_delete"));
    
    button_sizer->Add(add_button_, 0, wxRIGHT, 5);
    button_sizer->Add(edit_button_, 0, wxRIGHT, 5);
    button_sizer->Add(delete_button_, 0, wxRIGHT, 5);
    button_sizer->AddStretchSpacer();
    button_sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0);  // [SshAccountsDialog.md#standard-buttons]
    
    main_sizer->Add(button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(550, 300)));
    Fit();
    Centre();
    
    list_ctrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountsDialog::on_list_double_click, this);
    
    populate_list();
    update_buttons();
}

SshAccountsDialog::~SshAccountsDialog()
{}

void SshAccountsDialog::populate_list()
{
    list_ctrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().ssh_accounts_snapshot();
    long index = 0;
    for (const auto& account : accounts) {
        long item = list_ctrl_->InsertItem(index, wxString::FromUTF8(account.name()));  // [SshAccountsDialog.md#utf8-list]
        list_ctrl_->SetItem(item, 1, wxString::FromUTF8(account.webdav() ? account.url() : account.host()));  // [SshAccountsDialog.md#webdav-accounts]
        list_ctrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        list_ctrl_->SetItem(item, 3, wxString(account.password().empty() ? "" : "********"));
        index++;
    }
}

void SshAccountsDialog::update_buttons()
{
    bool has_selection = get_selected_index() != -1;
    edit_button_->Enable(has_selection);
    delete_button_->Enable(has_selection);
}

long SshAccountsDialog::get_selected_index() const
{
    return list_ctrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void SshAccountsDialog::on_add(wxCommandEvent&)
{
    SshAccount new_account;
    SshAccountDialog dlg(&new_account, this, wxID_ANY, txt::add_ssh_connection());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.save_data(new_account);
        Config::instance().add(new_account);
        populate_list();
    }
}

namespace {
bool edited(wxWindow* parent, SshAccount& account)  // [SshAccountsDialog.md#webdav-accounts]
{
    if (account.webdav()) {
        WebDavAccountDialog dlg(account, parent, wxID_ANY, txt::edit_webdav_connection());
        if (dlg.ShowModal() != wxID_OK)
            return false;
        dlg.save_data(account);
        return true;
    }
    SshAccountDialog dlg(&account, parent, wxID_ANY, txt::edit_ssh_connection());
    if (dlg.ShowModal() != wxID_OK)
        return false;
    dlg.save_data(account);
    return true;
}
}

void SshAccountsDialog::on_edit(wxCommandEvent&)
{
    edit_selected_item();
}

void SshAccountsDialog::on_delete(wxCommandEvent&)
{
    long sel = get_selected_index();
    if (sel == -1)
        return;

    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    wxString message = translation::translate("confirm_delete_ssh_account", {wxString::FromUTF8(account->name()).ToStdWstring()});
    if (Config::instance().ssh_account_referenced_by_any_item(account->name()))  // [SshAccountsDialog.md#deletion-warns]
        message += L"\n\n" + translation::translate("ssh_account_in_use_warning");

    if (!utilwx::confirm(this, message, translation::translate("confirm_delete_title"), translation::translate("button_delete")))
        return;
    Config::instance().remove_ssh_account(static_cast<size_t>(sel));
    populate_list();
    update_buttons();
}

void SshAccountsDialog::on_list_double_click(wxMouseEvent&)
{
    edit_selected_item();
}

void SshAccountsDialog::on_list_select(wxListEvent&)
{
    update_buttons();
}

void SshAccountsDialog::on_list_deselect(wxListEvent&)
{
    update_buttons();
}

void SshAccountsDialog::on_end_label_edit(wxListEvent& event)
{
    if (event.IsEditCancelled())
        return;

    long sel = event.GetIndex();
    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    if (auto err = validate_ssh_account_name(event.GetLabel())) {
        wxMessageBox(*err, translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }

    const std::string old_name = account->name();
    const std::string new_name = std::string(event.GetLabel().ToUTF8());
    if (new_name != old_name && Config::instance().find_ssh_account_by_name(new_name) != std::nullopt) {
        wxMessageBox(translation::translate("ssh_account_exists", {event.GetLabel().ToStdWstring()}),
            translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }
    account->set_name(new_name);
    Config::instance().update_ssh_account(static_cast<size_t>(sel), *account);
    Config::instance().update_ssh_account_references(old_name, *account);  // [SshAccountsDialog.md#account-references-updated]
}

void SshAccountsDialog::edit_selected_item()
{
    long sel = get_selected_index();
    if (sel == -1)
        return;

    auto account = Config::instance().ssh_account_at(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    const std::string old_name = account->name();
    if (edited(this, account.value())) {
        Config::instance().update_ssh_account(static_cast<size_t>(sel), account.value());
        Config::instance().update_ssh_account_references(old_name, account.value());  // [SshAccountsDialog.md#account-references-updated]
        populate_list();
    }
}
