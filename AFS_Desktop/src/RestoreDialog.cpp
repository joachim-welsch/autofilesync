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
#include "RestoreDialog.h"
#include <wx/dirdlg.h>
#include <wx/filename.h>
#include "Config.h"
#include "TextResources.h"
#include "Translation.h"

using namespace AFS_Core;

namespace fs = std::filesystem;
using translation::translate;

namespace {
constexpr int STATUS_WRAP_WIDTH = 860;  // [RestoreDialog.md#status]

wxString time_text(std::chrono::system_clock::time_point time)
{
    const wxDateTime local(std::chrono::system_clock::to_time_t(time));
    return local.FormatISODate() + " " + local.Format("%H:%M");
}

wxString folder_text(const Restorable& file)
{
    return restore::place_known(file) ? wxString(file.home.parent_path().wstring()) : wxString(translate("restore_folder_unknown"));
}

wxString kind_text(const Restorable& file)
{
    return translate(file.kind == RestorableKind::Deleted ? "restore_kind_deleted" : "restore_kind_version");
}
}

class RestoreList : public wxListView {  // [RestoreDialog.md#virtual-list]
public:
    RestoreList(RestoreDialog* dialog, wxWindowID id)
        : wxListView(dialog, id, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL), dialog_(dialog)
    {}
protected:
    wxString OnGetItemText(long row, long column) const override
    {
        return dialog_->cell_text(row, column);
    }
private:
    RestoreDialog* dialog_;
};

RestoreDialog::RestoreDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    filter_box_ = new wxSearchCtrl(this, ID_RESTORE_FILTER);
    filter_box_->SetDescriptiveText(translate("restore_filter_hint"));
    main_sizer->Add(filter_box_, 0, wxEXPAND | wxALL, 5);
    list_ = new RestoreList(this, ID_RESTORE_LIST);
    list_->AppendColumn(translate("restore_column_file"), wxLIST_FORMAT_LEFT, FromDIP(230));
    list_->AppendColumn(translate("restore_column_folder"), wxLIST_FORMAT_LEFT, FromDIP(310));
    list_->AppendColumn(translate("restore_column_kind"), wxLIST_FORMAT_LEFT, FromDIP(120));
    list_->AppendColumn(translate("restore_column_time"), wxLIST_FORMAT_LEFT, FromDIP(130));
    list_->AppendColumn(translate("size"), wxLIST_FORMAT_RIGHT, FromDIP(80));
    list_->SetMinSize(FromDIP(wxSize(890, 320)));
    main_sizer->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);
    status_ = new wxStaticText(this, ID_RESTORE_STATUS, translate("restore_searching"));
    main_sizer->Add(status_, 0, wxEXPAND | wxALL, 5);
    wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
    button_sizer->Add(new wxButton(this, ID_RESTORE_PUT_BACK, translate("restore_put_back")));
    button_sizer->Add(new wxButton(this, ID_RESTORE_PUT_ELSEWHERE, translate("restore_put_elsewhere")), wxSizerFlags().Border(wxLEFT, 5));
    button_sizer->AddStretchSpacer();
    button_sizer->Add(new wxButton(this, wxID_CANCEL, translate("button_close")));
    main_sizer->Add(button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    SetSizerAndFit(main_sizer);
    Centre();
    filter_box_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { apply_filter(); });
    Bind(wxEVT_BUTTON, &RestoreDialog::on_put_back, this, ID_RESTORE_PUT_BACK);
    Bind(wxEVT_BUTTON, &RestoreDialog::on_put_elsewhere, this, ID_RESTORE_PUT_ELSEWHERE);
    Bind(wxEVT_UPDATE_UI, &RestoreDialog::on_update_restore_button, this, ID_RESTORE_PUT_BACK);
    Bind(wxEVT_UPDATE_UI, &RestoreDialog::on_update_restore_button, this, ID_RESTORE_PUT_ELSEWHERE);
    start_search();
}

RestoreDialog::~RestoreDialog()  // [RestoreDialog.md#search-thread]
{
    stop_search_ = true;
    if (search_.joinable())
        search_.join();
}

void RestoreDialog::start_search()  // [RestoreDialog.md#search-thread]
{
    search_ = std::thread([this, items = Config::instance().syncitems()] {
        Restorables found = restore::find(items, stop_search_);
        if (!stop_search_)
            CallAfter([this, found = std::move(found)]() mutable { show_found(std::move(found)); });
    });
}

void RestoreDialog::show_found(Restorables found)
{
    found_ = std::move(found);
    searching_ = false;
    apply_filter();
    status_->SetLabel(found_.empty() ? translate("restore_nothing_found") : translate("restore_found", {std::to_wstring(found_.size())}));
    status_->Wrap(FromDIP(STATUS_WRAP_WIDTH));
    Layout();
}

bool RestoreDialog::matches_filter(const Restorable& file, const wxString& filter) const
{
    return wxString(file.home.filename().wstring()).Lower().Contains(filter) || folder_text(file).Lower().Contains(filter);
}

void RestoreDialog::apply_filter()  // [RestoreDialog.md#filter]
{
    const wxString filter = filter_box_->GetValue().Lower();
    for (long row = list_->GetFirstSelected(); row != -1; row = list_->GetFirstSelected())
        list_->Select(row, false);
    shown_.clear();
    list_->SetItemCount(0);
    for (std::size_t i = 0; i < found_.size(); ++i)
        if (filter.empty() || matches_filter(found_[i], filter))
            shown_.push_back(i);
    list_->SetItemCount(static_cast<long>(shown_.size()));
    list_->Refresh();
}

wxString RestoreDialog::cell_text(long row, long column) const  // [RestoreDialog.md#virtual-list]
{
    if (row < 0 || static_cast<std::size_t>(row) >= shown_.size())
        return wxEmptyString;
    const Restorable& file = found_[shown_[row]];
    switch (column) {
    case RESTORE_COLUMN_FILE: return file.home.filename().wstring();
    case RESTORE_COLUMN_FOLDER: return folder_text(file);
    case RESTORE_COLUMN_KIND: return kind_text(file);
    case RESTORE_COLUMN_TIME: return time_text(file.time);
    default: return wxFileName::GetHumanReadableSize(wxULongLong(file.size));
    }
}

std::vector<std::size_t> RestoreDialog::selected() const
{
    std::vector<std::size_t> files;
    for (long row = list_->GetFirstSelected(); row != -1; row = list_->GetNextSelected(row))
        files.push_back(shown_[row]);
    return files;
}

std::optional<fs::path> RestoreDialog::ask_directory()
{
    wxDirDialog dialog(this, translate("restore_choose_dir_title"), wxGetHomeDir(), wxDD_DEFAULT_STYLE);
    if (dialog.ShowModal() != wxID_OK)
        return std::nullopt;
    return fs::path(dialog.GetPath().ToStdWstring());
}

void RestoreDialog::put_back(bool ask_where)  // [RestoreDialog.md#put_back]
{
    const std::vector<std::size_t> files = selected();
    if (files.empty())
        return;
    const bool place_missing = std::ranges::any_of(files, [this](std::size_t i) { return !restore::place_known(found_[i]); });
    const std::optional<fs::path> asked = ask_where || place_missing ? ask_directory() : std::nullopt;
    if ((ask_where || place_missing) && !asked)
        return;
    std::vector<fs::path> restored;
    std::vector<std::size_t> taken_from_bin;
    for (const std::size_t i : files) {
        const Restorable& file = found_[i];
        const bool to_asked = ask_where || !restore::place_known(file);
        if (const auto place = restore::put_back(file, to_asked ? *asked : file.home.parent_path())) {
            restored.push_back(*place);
            if (file.kind == RestorableKind::Deleted)
                taken_from_bin.push_back(i);
        }
    }
    const Restorable last = found_[files.back()];
    forget_restored(taken_from_bin);
    report(restored, last, files.size() - restored.size());
}

void RestoreDialog::forget_restored(const std::vector<std::size_t>& restored)  // [RestoreDialog.md#put_back]
{
    shown_.clear();
    for (auto i = restored.rbegin(); i != restored.rend(); ++i)
        found_.erase(found_.begin() + static_cast<std::ptrdiff_t>(*i));
    apply_filter();
}

void RestoreDialog::report(const std::vector<fs::path>& restored, const Restorable& last, std::size_t failed)  // [RestoreDialog.md#status]
{
    if (failed > 0)
        wxMessageBox(translate("restore_failed", {std::to_wstring(failed)}), translate("error"), wxOK | wxICON_ERROR, this);
    if (restored.empty())
        return;
    const fs::path& place = restored.back();
    const bool renamed = restored.size() == 1 && place.filename() != last.home.filename();
    if (restored.size() > 1)
        status_->SetLabel(translate("restore_result_many", {std::to_wstring(restored.size())}));
    else
        status_->SetLabel(translate(renamed ? "restore_result_renamed" : "restore_result_one", {place.filename().wstring(), place.parent_path().wstring()}));
    status_->Wrap(FromDIP(STATUS_WRAP_WIDTH));
    Layout();
}

void RestoreDialog::on_put_back(wxCommandEvent&)
{
    put_back(false);
}

void RestoreDialog::on_put_elsewhere(wxCommandEvent&)
{
    put_back(true);
}

void RestoreDialog::on_update_restore_button(wxUpdateUIEvent& event)
{
    event.Enable(list_->GetSelectedItemCount() > 0);
}
