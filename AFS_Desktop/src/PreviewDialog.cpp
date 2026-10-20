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
#include "PreviewDialog.h"
#include "Config.h"
#include "Global.h"
#include "SyncBase.h"
#include "Translation.h"
#include "Util.h"
#include <wx/filename.h>
#include <filesystem>

using namespace AFS_Core;
using translation::translate;
using std::wstring;

namespace {
constexpr int ACTION_COLUMN_WIDTH = 200;
constexpr int ITEM_COLUMN_WIDTH = 160;
constexpr int FILE_COLUMN_WIDTH = 320;
constexpr int SIZE_COLUMN_WIDTH = 80;

wxString human_size(uint64_t bytes)
{
    return wxFileName::GetHumanReadableSize(wxULongLong(bytes), translate("preview_no_bytes"), 1);
}

wstring relative_to_root(const wstring& path, const SyncItem& item, bool in_target)
{
    if (item.sync_item_type() == SyncItemType::File)
        return std::filesystem::path(path).filename().wstring();
    const bool ssh = in_target ? item.ssh_target() : item.ssh_source();
    const wstring root = util::with_trailing_char(without_volume_spec(in_target ? item.target() : item.source()), ssh ? L'/' : DIR_SEPARATOR);
    return path.starts_with(root) ? path.substr(root.size()) : path;
}

wstring shown_path(const PlannedStep& step)  // [PreviewDialog.md#shown_path]
{
    const bool in_target = file_in_target(step.action);
    const std::optional<SyncItem> item = Config::instance().find_by_name(step.item);
    if (!item)
        return rename(step.action) ? step.source + L" \u2192 " + step.target : in_target ? step.target : step.source;
    if (rename(step.action))  // [PreviewDialog.md#renames]
        return relative_to_root(step.source, *item, in_target) + L" \u2192 " + relative_to_root(step.target, *item, in_target);
    return relative_to_root(in_target ? step.target : step.source, *item, in_target);
}
}

PreviewDialog::PreviewDialog(wxWindow* parent, const SyncPlan& plan, const wxString& notes)
    : wxDialog(parent, wxID_ANY, translate("preview_title"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(new wxStaticText(this, wxID_ANY, translate("preview_intro")), 0, wxEXPAND | wxALL, 10);
    list_ = new wxListCtrl(this, ID_PREVIEW_LIST, wxDefaultPosition, FromDIP(wxSize(800, 320)), wxLC_REPORT | wxLC_SINGLE_SEL);
    list_->InsertColumn(0, translate("preview_column_action"), wxLIST_FORMAT_LEFT, FromDIP(ACTION_COLUMN_WIDTH));
    list_->InsertColumn(1, translate("synchronisation"), wxLIST_FORMAT_LEFT, FromDIP(ITEM_COLUMN_WIDTH));
    list_->InsertColumn(2, translate("preview_column_file"), wxLIST_FORMAT_LEFT, FromDIP(FILE_COLUMN_WIDTH));
    list_->InsertColumn(3, translate("size"), wxLIST_FORMAT_RIGHT, FromDIP(SIZE_COLUMN_WIDTH));
    main_sizer->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    fill(plan);
    main_sizer->Add(new wxStaticText(this, wxID_ANY, summary(plan)), 0, wxEXPAND | wxALL, 10);
    if (!notes.empty())  // [PreviewDialog.md#notes]
        main_sizer->Add(new wxStaticText(this, wxID_ANY, notes), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel(translate("preview_run_now"));
    FindWindow(wxID_CANCEL)->SetLabel(translate("button_close"));
    dynamic_cast<wxButton*>(FindWindow(wxID_CANCEL))->SetDefault();  // [PreviewDialog.md#close-is-the-default]
    FindWindow(wxID_CANCEL)->SetFocus();
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);
    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(600, 300)));
    Fit();
    Centre();
}

void PreviewDialog::fill(const SyncPlan& plan)
{
    for (const PlannedStep& step : plan.steps())
        add_step(step);
    if (plan.not_listed() > 0) {  // [SyncPlan.md#MAX_STEPS]
        const long row = list_->InsertItem(list_->GetItemCount(), translate("preview_more", {std::to_wstring(plan.not_listed())}));
        list_->SetItemFont(row, list_->GetFont().Italic());
    }
}

void PreviewDialog::add_step(const PlannedStep& step)
{
    const long row = list_->InsertItem(list_->GetItemCount(), action_label(step.action));
    list_->SetItem(row, 1, step.item);
    list_->SetItem(row, 2, shown_path(step));
    list_->SetItem(row, 3, deletion(step.action) || rename(step.action) ? wxString() : human_size(step.bytes));
    if (deletion(step.action))  // [PreviewDialog.md#deletions-in-red]
        list_->SetItemTextColour(row, *wxRED);
}

wxString PreviewDialog::action_label(PlannedAction action)
{
    switch (action) {
    case PlannedAction::CopyToTarget: return translate("preview_copy_to_target");
    case PlannedAction::MoveToTarget: return translate("preview_move_to_target");
    case PlannedAction::CopyToSource: return translate("preview_copy_to_source");
    case PlannedAction::DeleteInSource: return translate("preview_delete_in_source");
    case PlannedAction::RenameInTarget: return translate("preview_rename_in_target");
    case PlannedAction::RenameInSource: return translate("preview_rename_in_source");
    case PlannedAction::ConflictCopyToTarget: return translate("preview_conflict_to_target");
    case PlannedAction::ConflictCopyToSource: return translate("preview_conflict_to_source");
    case PlannedAction::DeleteInTarget: break;
    }
    return translate("preview_delete_in_target");
}

wxString PreviewDialog::summary(const SyncPlan& plan)  // [PreviewDialog.md#summary]
{
    return translate("preview_summary", {std::to_wstring(plan.count()), std::to_wstring(plan.deletions()),
        human_size(plan.bytes()).ToStdWstring()});
}
