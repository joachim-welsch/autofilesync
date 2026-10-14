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
#include "SshDirDialog.h"
#include "SshDirInspector.h"
#include "Translation.h"
#include <wx/busyinfo.h>
#include <algorithm>

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshDirDialog, wxDialog)
    EVT_BUTTON(ID_SSH_DIR_HOME_BUTTON, SshDirDialog::on_home_button)
    EVT_BUTTON(ID_SSH_DIR_SORT_BUTTON, SshDirDialog::on_sort_by_name)
    EVT_TEXT_ENTER(ID_SSH_DIR_PATH_BOX, SshDirDialog::on_path_enter)
    EVT_LIST_ITEM_SELECTED(ID_SSH_DIR_LIST, SshDirDialog::on_list_select)
END_EVENT_TABLE()

SshDirDialog::SshDirDialog(wxWindow* parent, 
    const wxString& title,
    const SshAccount& account,
    const wxString& default_path,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxDialog(parent, wxID_ANY, title, pos, size, style)
    , account_(account)
    , current_path_(default_path)
    , selected_path_(default_path)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* conn_info = new wxStaticText(this, wxID_STATIC, 
        translation::translate("ssh_connection_info", {wxString::FromUTF8(account_.user()).ToStdWstring(), wxString::FromUTF8(account_.host()).ToStdWstring()}));
    main_sizer->Add(conn_info, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* path_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    home_button_ = new wxButton(this, ID_SSH_DIR_HOME_BUTTON, translation::translate("home"),
        wxDefaultPosition, FromDIP(wxSize(50, -1)));
    path_sizer->Add(home_button_, 0, wxRIGHT, 2);

    sort_button_ = new wxButton(this, ID_SSH_DIR_SORT_BUTTON, "A-Z",
        wxDefaultPosition, FromDIP(wxSize(35, -1)));
    path_sizer->Add(sort_button_, 0, wxRIGHT, 5);

    path_box_ = new wxTextCtrl(this, ID_SSH_DIR_PATH_BOX, current_path_,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    path_sizer->Add(path_box_, 1, wxEXPAND);
    
    main_sizer->Add(path_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    list_ctrl_ = new wxListCtrl(this, ID_SSH_DIR_LIST, wxDefaultPosition,
        FromDIP(wxSize(500, 300)), wxLC_REPORT | wxLC_SINGLE_SEL);
    
    list_ctrl_->InsertColumn(0, translation::translate("name"), wxLIST_FORMAT_LEFT, 250);
    list_ctrl_->InsertColumn(1, translation::translate("size"), wxLIST_FORMAT_RIGHT, 80);
    list_ctrl_->InsertColumn(2, translation::translate("modified"), wxLIST_FORMAT_LEFT, 150);
    
    main_sizer->Add(list_ctrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    
    list_ctrl_->Bind(wxEVT_LEFT_DCLICK, &SshDirDialog::on_list_double_click, this);

    status_text_ = new wxStaticText(this, wxID_STATIC, wxEmptyString);
    main_sizer->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(550, 400)));
    Fit();
    Centre();

    populate_list(current_path_);
}

SshDirDialog::~SshDirDialog()
{}

void SshDirDialog::populate_list(const wxString& path)
{
    list_ctrl_->DeleteAllItems();
    status_text_->SetLabel(translation::translate("loading_dir"));
    Update();

    try {
        wxBusyCursor wait;
        SshDirInspector inspector(account_);
        RemoteDirInfo info = inspector.inspect_dir(std::string(path.ToUTF8()));

        if (!info.exists) {
            status_text_->SetLabel(translation::translate("error_with_reason", {wxString::FromUTF8(info.error_message).ToStdWstring()}));
            return;
        }

        if (!info.is_directory) {
            status_text_->SetLabel(translation::translate("not_a_directory"));
            return;
        }

        current_path_ = path;
        path_box_->SetValue(current_path_);

        entries_ = std::move(info.entries);
        render_list();

        status_text_->SetLabel(translation::translate("entries_count", {std::to_wstring(entries_.size())}));

    } catch (const std::exception& ex) {
        status_text_->SetLabel(translation::translate("error_with_reason", {wxString::FromUTF8(ex.what()).ToStdWstring()}));
    }
}

void SshDirDialog::render_list()
{
    list_ctrl_->DeleteAllItems();

    long index = 0;
    for (const auto& entry : entries_) {
        wxString display_name = wxString::FromUTF8(entry.name);  // remote file names arrive as UTF-8 from the SFTP server
        if (entry.is_directory) {
            display_name = "[" + display_name + "]";
        }

        long item = list_ctrl_->InsertItem(index, display_name);

        if (entry.is_regular_file) {
            wxString size_str;
            if (entry.size_bytes < 1024) {
                size_str = wxString::Format("%llu B", entry.size_bytes);
            } else if (entry.size_bytes < 1024 * 1024) {
                size_str = wxString::Format("%.1f KB", entry.size_bytes / 1024.0);
            } else {
                size_str = wxString::Format("%.1f MB", entry.size_bytes / (1024.0 * 1024.0));
            }
            list_ctrl_->SetItem(item, 1, size_str);
        } else if (entry.is_directory) {
            list_ctrl_->SetItem(item, 1, "<DIR>");
        }

        list_ctrl_->SetItem(item, 2, entry.last_modified_iso);

        list_ctrl_->SetItemData(item, entry.is_directory ? 1 : 0);

        index++;
    }
}

void SshDirDialog::on_list_double_click(wxMouseEvent&)
{
    long sel = list_ctrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1)
        return;
    
    if (list_ctrl_->GetItemData(sel) == 1) {
        wxString item_text = list_ctrl_->GetItemText(sel);
        if (item_text.StartsWith("[") && item_text.EndsWith("]")) {
            item_text = item_text.Mid(1, item_text.Length() - 2);
        }
        
        wxString new_path = current_path_;
        if (!new_path.EndsWith("/")) {
            new_path += "/";
        }
        new_path += item_text;
        
        populate_list(new_path);
    }
}

void SshDirDialog::on_list_select(wxListEvent& event)
{
    long sel = event.GetIndex();
    if (sel == -1)
        return;
    
    wxString item_text = list_ctrl_->GetItemText(sel);
    if (item_text.StartsWith("[") && item_text.EndsWith("]")) {
        item_text = item_text.Mid(1, item_text.Length() - 2);
    }
    
    selected_path_ = current_path_;
    if (!selected_path_.EndsWith("/")) {
        selected_path_ += "/";
    }
    selected_path_ += item_text;
}

void SshDirDialog::on_path_enter(wxCommandEvent&)
{
    wxString new_path = path_box_->GetValue();
    if (!new_path.IsEmpty()) {
        populate_list(new_path);
    }
}

void SshDirDialog::on_home_button(wxCommandEvent&)
{
    try {
        wxBusyCursor wait;
        SshDirInspector inspector(account_);
        populate_list(wxString::FromUTF8(inspector.get_home_dir()));
    } catch (const std::exception& ex) {
        status_text_->SetLabel(translation::translate("error_with_reason", {wxString::FromUTF8(ex.what()).ToStdWstring()}));
    }
}

void SshDirDialog::on_sort_by_name(wxCommandEvent&)
{
    bool ascending = sort_ascending_;
    std::sort(entries_.begin(), entries_.end(), [ascending](const RemoteDirEntry& a, const RemoteDirEntry& b) {
        int cmp = wxString::FromUTF8(a.name).CmpNoCase(wxString::FromUTF8(b.name));
        return ascending ? cmp < 0 : cmp > 0;
    });
    sort_ascending_ = !sort_ascending_;
    render_list();
}
