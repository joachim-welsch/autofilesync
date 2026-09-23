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

using namespace AFS_Core;

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
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->AddStretchSpacer()->SetMinSize(20, 10);

    wxBoxSizer* startAutostartSizer = new wxBoxSizer(wxHORIZONTAL);
    autoStartCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_autostart"));
    startAutostartSizer->Add(autoStartCB_, 1);
    mainSizer->Add(startAutostartSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* startSyncAtStartSizer = new wxBoxSizer(wxHORIZONTAL);
    startSyncAtStartCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_sync_at_start"));
    startSyncAtStartSizer->Add(startSyncAtStartCB_, 1);
    mainSizer->Add(startSyncAtStartSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* startMinimizedSizer = new wxBoxSizer(wxHORIZONTAL);
    startMinimizedCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_start_minimized"));
    startMinimizedSizer->Add(startMinimizedCB_, 1);
    mainSizer->Add(startMinimizedSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* minimizeInsteadOfCloseSizer = new wxBoxSizer(wxHORIZONTAL);
    minimizeInsteadOfCloseCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("settings_minimize_instead_of_close"));
    minimizeInsteadOfCloseSizer->Add(minimizeInsteadOfCloseCB_, 1);
    mainSizer->Add(minimizeInsteadOfCloseSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* showBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* showBoxLable = new wxStaticText(this, wxID_STATIC, translation::translate("settings_show_messages"));
    mainSizer->Add(showBoxSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 5);
    showBoxSizer->Add(showBoxLable, 1);
    wxBoxSizer* showMessageBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    messageBoxErrorCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("error"));
    showMessageBoxSizer->Add(messageBoxErrorCB_);
    messageBoxWarningCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("warning"));
    showMessageBoxSizer->Add(messageBoxWarningCB_);
    messageBoxInfoCB_ = new wxCheckBox(this, wxID_ANY, translation::translate("info"));
    showMessageBoxSizer->Add(messageBoxInfoCB_);
    mainSizer->Add(showMessageBoxSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    const wxString deleteModes[] = { translation::translate("delete_mode_recycle_bin"), translation::translate("delete_mode_permanent") };
    deleteModeRB_ = new wxRadioBox(this, ID_DELETE_MODE_RADIO, translation::translate("delete_mode"), wxDefaultPosition, wxDefaultSize,
        WXSIZEOF(deleteModes), deleteModes, 1, wxRA_SPECIFY_COLS);
    deleteModeRB_->Bind(wxEVT_RADIOBOX, &SettingsDialog::onDeleteModeChanged, this);
    mainSizer->Add(deleteModeRB_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* recycleBinSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* recycleBinLable = new wxStaticText(this, wxID_STATIC, translation::translate("recycle_bin"));
    recycleBinSizer->Add(recycleBinLable, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    recycleBinPathBox_ = new wxTextCtrl(this, ID_RECYCLEBIN_PATH_BOX);
    recycleBinPathBox_->SetMinSize(wxSize(250, recycleBinPathBox_->GetMinSize().y));
    recycleBinSizer->Add(recycleBinPathBox_, 1);
    recycleBinPathBtn_ = new wxButton(this, ID_RECYCLEBIN_PATH_BUTTON, "...", wxDefaultPosition, wxSize(40, -1));
    recycleBinSizer->Add(recycleBinPathBtn_, wxSizerFlags().Border(wxLEFT, 2));
    recycleBinPathBtn_->Bind(wxEVT_BUTTON, &SettingsDialog::onPickRecycleBinPath, this);
    mainSizer->Add(recycleBinSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(250, 150));
    Fit();
    Centre();
}

SettingsDialog::~SettingsDialog()
{}

bool SettingsDialog::TransferDataToWindow()
{
    const Settings& s = Config::instance().settings();
    startMinimizedCB_->SetValue(s.start_minimized());
    minimizeInsteadOfCloseCB_->SetValue(s.minimize_instead_of_close());
    autoStartCB_->SetValue(s.auto_start());
    startSyncAtStartCB_->SetValue(s.start_sync_at_start());
    messageBoxErrorCB_->SetValue(s.show_errors());
    messageBoxWarningCB_->SetValue(s.show_warnings());
    messageBoxInfoCB_->SetValue(s.show_infos());
    recycleBinPathBox_->SetValue(RecycleBin::instance().bin_path().wstring());
    deleteModeRB_->SetSelection(s.delete_mode() == DeleteMode::Permanent ? 1 : 0);
    enableRecycleBinPath();
    return true;
}

void SettingsDialog::enableRecycleBinPath()  // [SettingsDialog.md#enableRecycleBinPath]
{
    const bool usesBin = deleteModeRB_->GetSelection() == 0;
    recycleBinPathBox_->Enable(usesBin);
    recycleBinPathBtn_->Enable(usesBin);
}

void SettingsDialog::onDeleteModeChanged(wxCommandEvent&)
{
    enableRecycleBinPath();
}

void SettingsDialog::onPickRecycleBinPath(wxCommandEvent&)  // [SettingsDialog.md#onPickRecycleBinPath]
{
    wxString start = recycleBinPathBox_->GetValue();
    if (start.empty() || !wxDirExists(start))
        start = wxGetHomeDir();
    wxDirDialog dialog(this, translation::translate("choose_recycle_bin_dir"), start, wxDD_DEFAULT_STYLE);
    if (dialog.ShowModal() == wxID_OK)
        recycleBinPathBox_->SetValue(dialog.GetPath());
}

bool SettingsDialog::TransferDataFromWindow()
{
    bool changed = false;
    Settings& s = Config::instance().settings();
    if (s.start_minimized() != startMinimizedCB_->GetValue()) {
        s.start_minimized(startMinimizedCB_->GetValue());
        changed = true;
    }
    if (s.minimize_instead_of_close() != minimizeInsteadOfCloseCB_->GetValue()) {
        s.minimize_instead_of_close(minimizeInsteadOfCloseCB_->GetValue());
        changed = true;
    }
    if (s.auto_start() != autoStartCB_->GetValue()) {
        s.auto_start(autoStartCB_->GetValue());
        changed = true;
    }
    if (s.start_sync_at_start() != startSyncAtStartCB_->GetValue()) {
        s.start_sync_at_start(startSyncAtStartCB_->GetValue());
        changed = true;
    }
    if (s.show_errors() != messageBoxErrorCB_->GetValue()) {
        s.show_errors(messageBoxErrorCB_->GetValue());
        changed = true;
    }
    if (s.show_warnings() != messageBoxWarningCB_->GetValue()) {
        s.show_warnings(messageBoxWarningCB_->GetValue());
        changed = true;
    }
    if (s.show_infos() != messageBoxInfoCB_->GetValue()) {
        s.show_infos(messageBoxInfoCB_->GetValue());
        changed = true;
    }
    const DeleteMode deleteMode = deleteModeRB_->GetSelection() == 1 ? DeleteMode::Permanent : DeleteMode::RecycleBin;
    if (s.delete_mode() != deleteMode) {
        s.delete_mode(deleteMode);
        changed = true;
    }
    const std::wstring recycleBinPath = recycleBinPathBox_->GetValue().ToStdWstring();
    if (!recycleBinPath.empty() && RecycleBin::instance().bin_path() != std::filesystem::path(recycleBinPath)) {
        RecycleBin::instance().bin_path(recycleBinPath);
        changed = true;
    }
    if (changed)
        Config::instance().set_changed();
    return true;
}

