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
#include "SettingsDialog.h"
#include "Config.h"
#include "RecycleBin.h"
#include "Translation.h"
#include <wx/dirdlg.h>
#include <algorithm>

using namespace AFS_Core;

namespace {
int language_index(const std::string& code)  // [SettingsDialog.md#language_index]
{
    const auto& languages = translation::LANGUAGES;
    const auto found = std::ranges::find(languages, code, &translation::Language::code);
    return found == languages.end() ? 0 : static_cast<int>(found - languages.begin()) + 1;
}

std::string language_code(int index)
{
    return index <= 0 ? std::string() : translation::LANGUAGES[static_cast<std::size_t>(index - 1)].code;
}
}

BEGIN_EVENT_TABLE(SettingsDialog, wxDialog)
END_EVENT_TABLE()

SettingsDialog::SettingsDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(20, 10)));
    add_language_choice(main_sizer);

    wxBoxSizer* start_autostart_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto_start_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_autostart"));
    start_autostart_sizer->Add(auto_start_check_, 1);
    main_sizer->Add(start_autostart_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* start_sync_at_start_sizer = new wxBoxSizer(wxHORIZONTAL);
    start_sync_at_start_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_sync_at_start"));
    start_sync_at_start_sizer->Add(start_sync_at_start_check_, 1);
    main_sizer->Add(start_sync_at_start_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* start_minimized_sizer = new wxBoxSizer(wxHORIZONTAL);
    start_minimized_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_start_minimized"));
    start_minimized_sizer->Add(start_minimized_check_, 1);
    main_sizer->Add(start_minimized_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* minimize_instead_of_close_sizer = new wxBoxSizer(wxHORIZONTAL);
    minimize_instead_of_close_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_minimize_instead_of_close"));
    minimize_instead_of_close_sizer->Add(minimize_instead_of_close_check_, 1);
    main_sizer->Add(minimize_instead_of_close_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* show_box_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* show_box_lable = new wxStaticText(this, wxID_STATIC, translation::translate("settings_show_messages"));
    main_sizer->Add(show_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 5);
    show_box_sizer->Add(show_box_lable, 1);
    wxBoxSizer* show_message_box_sizer = new wxBoxSizer(wxHORIZONTAL);
    message_box_error_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("error"));
    show_message_box_sizer->Add(message_box_error_check_);
    message_box_warning_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("warning"));
    show_message_box_sizer->Add(message_box_warning_check_);
    message_box_info_check_ = new wxCheckBox(this, wxID_ANY, translation::translate("info"));
    show_message_box_sizer->Add(message_box_info_check_);
    main_sizer->Add(show_message_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    const wxString delete_modes[] = { translation::translate("delete_mode_recycle_bin"), translation::translate("delete_mode_permanent") };
    delete_mode_radio_ = new wxRadioBox(this, ID_DELETE_MODE_RADIO, translation::translate("delete_mode"), wxDefaultPosition, wxDefaultSize,
        WXSIZEOF(delete_modes), delete_modes, 1, wxRA_SPECIFY_COLS);
    delete_mode_radio_->Bind(wxEVT_RADIOBOX, &SettingsDialog::on_delete_mode_changed, this);
    main_sizer->Add(delete_mode_radio_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* recycle_bin_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* recycle_bin_lable = new wxStaticText(this, wxID_STATIC, translation::translate("recycle_bin"));
    recycle_bin_sizer->Add(recycle_bin_lable, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    recycle_bin_path_box_ = new wxTextCtrl(this, ID_RECYCLEBIN_PATH_BOX);
    recycle_bin_path_box_->SetMinSize(wxSize(FromDIP(250), recycle_bin_path_box_->GetMinSize().y));
    recycle_bin_sizer->Add(recycle_bin_path_box_, 1);
    recycle_bin_path_button_ = new wxButton(this, ID_RECYCLEBIN_PATH_BUTTON, translation::translate("button_choose"));
    recycle_bin_sizer->Add(recycle_bin_path_button_, wxSizerFlags().Border(wxLEFT, 2));
    recycle_bin_path_button_->Bind(wxEVT_BUTTON, &SettingsDialog::on_pick_recycle_bin_path, this);
    main_sizer->Add(recycle_bin_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM, 5);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(250, 150)));
    Fit();
    Centre();
}

SettingsDialog::~SettingsDialog()
{}

void SettingsDialog::add_language_choice(wxSizer* sizer)
{
    wxBoxSizer* language_sizer = new wxBoxSizer(wxHORIZONTAL);
    language_sizer->Add(new wxStaticText(this, wxID_STATIC, translation::translate("settings_language")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    language_choice_ = new wxChoice(this, ID_LANGUAGE_CHOICE);
    language_choice_->Append(translation::translate("language_system"));
    for (const translation::Language& language : translation::LANGUAGES)
        language_choice_->Append(wxString::FromUTF8(language.name));
    language_sizer->Add(language_choice_, 1);
    sizer->Add(language_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
}

bool SettingsDialog::TransferDataToWindow()
{
    const Settings& s = Config::instance().settings();
    start_minimized_check_->SetValue(s.start_minimized());
    minimize_instead_of_close_check_->SetValue(s.minimize_instead_of_close());
    auto_start_check_->SetValue(s.auto_start());
    start_sync_at_start_check_->SetValue(s.start_sync_at_start());
    message_box_error_check_->SetValue(s.show_errors());
    message_box_warning_check_->SetValue(s.show_warnings());
    message_box_info_check_->SetValue(s.show_infos());
    recycle_bin_path_box_->SetValue(RecycleBin::instance().bin_path().wstring());
    delete_mode_radio_->SetSelection(s.delete_mode() == DeleteMode::Permanent ? 1 : 0);
    language_choice_->SetSelection(language_index(s.language()));
    enable_recycle_bin_path();
    return true;
}

void SettingsDialog::enable_recycle_bin_path()  // [SettingsDialog.md#enable_recycle_bin_path]
{
    const bool uses_bin = delete_mode_radio_->GetSelection() == 0;
    recycle_bin_path_box_->Enable(uses_bin);
    recycle_bin_path_button_->Enable(uses_bin);
}

void SettingsDialog::on_delete_mode_changed(wxCommandEvent&)
{
    enable_recycle_bin_path();
}

void SettingsDialog::on_pick_recycle_bin_path(wxCommandEvent&)  // [SettingsDialog.md#on_pick_recycle_bin_path]
{
    wxString start = recycle_bin_path_box_->GetValue();
    if (start.empty() || !wxDirExists(start))
        start = wxGetHomeDir();
    wxDirDialog dialog(this, translation::translate("choose_recycle_bin_dir"), start, wxDD_DEFAULT_STYLE);
    if (dialog.ShowModal() == wxID_OK)
        recycle_bin_path_box_->SetValue(dialog.GetPath());
}

bool SettingsDialog::TransferDataFromWindow()
{
    bool changed = false;
    Settings& s = Config::instance().settings();
    if (s.start_minimized() != start_minimized_check_->GetValue()) {
        s.start_minimized(start_minimized_check_->GetValue());
        changed = true;
    }
    if (s.minimize_instead_of_close() != minimize_instead_of_close_check_->GetValue()) {
        s.minimize_instead_of_close(minimize_instead_of_close_check_->GetValue());
        changed = true;
    }
    if (s.auto_start() != auto_start_check_->GetValue()) {
        s.auto_start(auto_start_check_->GetValue());
        changed = true;
    }
    if (s.start_sync_at_start() != start_sync_at_start_check_->GetValue()) {
        s.start_sync_at_start(start_sync_at_start_check_->GetValue());
        changed = true;
    }
    if (s.show_errors() != message_box_error_check_->GetValue()) {
        s.show_errors(message_box_error_check_->GetValue());
        changed = true;
    }
    if (s.show_warnings() != message_box_warning_check_->GetValue()) {
        s.show_warnings(message_box_warning_check_->GetValue());
        changed = true;
    }
    if (s.show_infos() != message_box_info_check_->GetValue()) {
        s.show_infos(message_box_info_check_->GetValue());
        changed = true;
    }
    const DeleteMode delete_mode = delete_mode_radio_->GetSelection() == 1 ? DeleteMode::Permanent : DeleteMode::RecycleBin;
    if (s.delete_mode() != delete_mode) {
        s.delete_mode(delete_mode);
        changed = true;
    }
    const std::wstring recycle_bin_path = recycle_bin_path_box_->GetValue().ToStdWstring();
    if (!recycle_bin_path.empty() && RecycleBin::instance().bin_path() != std::filesystem::path(recycle_bin_path)) {
        RecycleBin::instance().bin_path(recycle_bin_path);
        changed = true;
    }
    if (language_choice_->GetSelection() != language_index(s.language())) {  // [SettingsDialog.md#language_index]
        s.language(language_code(language_choice_->GetSelection()));
        changed = true;
    }
    if (changed)
        Config::instance().set_changed();
    return true;
}

