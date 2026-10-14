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
#include "LoggingDialog.h"
#include "Logging.h"
#include "id.h"
#include "Util.h"
#include "Config.h"
#include "LogCut.h"
#include "Translation.h"
#include <wx/dirdlg.h>

using namespace AFS_Core;

using util::wstring_to_string;

BEGIN_EVENT_TABLE(LoggingDialog, wxDialog)
    EVT_CHECKBOX(ID_EMAIL_ACTIVE_CHECK, LoggingDialog::on_notify_active_changed)
    EVT_BUTTON(ID_BTN_PICK_BACKUP_PROOF_PATH, LoggingDialog::on_pick_backup_proof_path)
END_EVENT_TABLE()

const wxString CHOICES[] = { "Debug", "Info", "Warning", "Error", "FatalError" };  // [LoggingDialog.md#CHOICES]

LoggingDialog::LoggingDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);
    notebook->AddPage(create_logging_page(notebook), translation::translate("tab_logging"), true);
    notebook->AddPage(create_email_page(notebook), translation::translate("tab_email"));
    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    wxSizer* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxBOTTOM, 5);

    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(300, 100)));
    Fit();
    Centre();
}

wxPanel* LoggingDialog::create_logging_page(wxNotebook* notebook)
{
    wxPanel* page = new wxPanel(notebook);
    wxBoxSizer* page_sizer = new wxBoxSizer(wxVERTICAL);
    page_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(300, 20)));

    const int label_width = page->GetTextExtent(translation::translate("backup_proof_path")).x + 5;  // [LoggingDialog.md#label_width]

    wxBoxSizer* log_path_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* name_lable = new wxStaticText(page, wxID_STATIC, translation::translate("log_file_path"));
    name_lable->SetMinSize(wxSize(label_width, name_lable->GetMinSize().y));
    log_path_sizer->Add(name_lable, 0, wxTOP, 3);
    log_path_box_ = new wxTextCtrl(page, ID_LOG_PATH_BOX);
    log_path_box_->SetMinSize(wxSize(FromDIP(250), log_path_box_->GetMinSize().y));
    log_path_sizer->Add(log_path_box_);
    page_sizer->Add(log_path_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* double_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* level_lable = new wxStaticText(page, wxID_STATIC, translation::translate("log_level"));
    level_lable->SetMinSize(wxSize(label_width, level_lable->GetMinSize().y));
    double_sizer->Add(level_lable, 0, wxEXPAND | wxLEFT, 0);
    log_level_choice_ = new wxChoice(page, ID_LOG_LEVEL_CHOICE, wxDefaultPosition, wxDefaultSize, WXSIZEOF(CHOICES), CHOICES);
    double_sizer->Add(log_level_choice_);

    retention_days_ctrl_ = new wxSpinCtrl(page, ID_RETENTIONDAYS_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 60, 1);
    wxBoxSizer* ret_lable_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* ret_lable_text_ctrl = new wxStaticText(page, wxID_STATIC, translation::translate("retention_days"));
    ret_lable_sizer->Add(ret_lable_text_ctrl, 0, wxEXPAND | wxTOP, 3);
    double_sizer->Add(ret_lable_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);;
    double_sizer->Add(retention_days_ctrl_);
    page_sizer->Add(double_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* edit_path_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* edit_lable = new wxStaticText(page, wxID_STATIC, translation::translate("editor"));
    edit_lable->SetMinSize(wxSize(label_width, edit_lable->GetMinSize().y));
    edit_path_sizer->Add(edit_lable, 0, wxTOP, 3);
    edit_path_box_ = new wxTextCtrl(page, ID_EDIT_PATH_BOX);
    edit_path_box_->SetMinSize(wxSize(FromDIP(250), edit_path_box_->GetMinSize().y));
    edit_path_sizer->Add(edit_path_box_);
    page_sizer->Add(edit_path_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* backup_proof_path_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* backup_proof_lable = new wxStaticText(page, wxID_STATIC, translation::translate("backup_proof_path"));
    backup_proof_lable->SetMinSize(wxSize(label_width, backup_proof_lable->GetMinSize().y));
    backup_proof_path_sizer->Add(backup_proof_lable, 0, wxTOP, 3);
    backup_proof_path_box_ = new wxTextCtrl(page, ID_BACKUP_PROOF_PATH_BOX);
    backup_proof_path_box_->SetMinSize(wxSize(FromDIP(250), backup_proof_path_box_->GetMinSize().y));
    backup_proof_path_sizer->Add(backup_proof_path_box_);
    backup_proof_path_sizer->Add(new wxButton(page, ID_BTN_PICK_BACKUP_PROOF_PATH, translation::translate("button_choose")), wxSizerFlags().Border(wxLEFT, 2));
    page_sizer->Add(backup_proof_path_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    page->SetSizer(page_sizer);
    return page;
}

void LoggingDialog::add_field(wxPanel* page, wxSizer* page_sizer, const wxString& label,
    wxWindowID id, wxTextCtrl*& box, long style)
{
    wxBoxSizer* field_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* text = new wxStaticText(page, wxID_STATIC, label);
    text->SetMinSize(wxSize(FromDIP(110), text->GetMinSize().y));
    field_sizer->Add(text, 0, wxTOP, 3);
    box = new wxTextCtrl(page, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, style);
    box->SetMinSize(wxSize(FromDIP(250), box->GetMinSize().y));
    field_sizer->Add(box);
    page_sizer->Add(field_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
}

wxPanel* LoggingDialog::create_email_page(wxNotebook* notebook)
{
    wxPanel* page = new wxPanel(notebook);
    wxBoxSizer* page_sizer = new wxBoxSizer(wxVERTICAL);
    page_sizer->AddStretchSpacer()->SetMinSize(FromDIP(wxSize(300, 20)));
    email_active_check_ = new wxCheckBox(page, ID_EMAIL_ACTIVE_CHECK, translation::translate("active"));
    page_sizer->Add(email_active_check_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    add_field(page, page_sizer, translation::translate("email_address"), ID_EMAIL_BOX, email_box_);

    page_sizer->Add(new wxStaticText(page, wxID_STATIC, translation::translate("smtp_server_group")),
        0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    add_field(page, page_sizer, translation::translate("server"), ID_SMTP_SERVER_BOX, smtp_server_box_);

    wxBoxSizer* port_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* port_label = new wxStaticText(page, wxID_STATIC, translation::translate("port"));
    port_label->SetMinSize(wxSize(FromDIP(110), port_label->GetMinSize().y));
    port_sizer->Add(port_label, 0, wxTOP, 3);
    smtp_port_ctrl_ = new wxSpinCtrl(page, ID_SMTP_PORT_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 65535, 587);
    port_sizer->Add(smtp_port_ctrl_);
    page_sizer->Add(port_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    add_field(page, page_sizer, translation::translate("user_field"), ID_SMTP_USER_BOX, smtp_user_box_);
    add_field(page, page_sizer, translation::translate("password_field"), ID_SMTP_PASSWORD_BOX, smtp_password_box_, wxTE_PASSWORD);
    add_field(page, page_sizer, translation::translate("sender"), ID_SMTP_FROM_BOX, smtp_from_box_);

    page->SetSizer(page_sizer);
    return page;
}

void LoggingDialog::on_notify_active_changed(wxCommandEvent&)
{
    update_notify_enabling();
}

void LoggingDialog::on_pick_backup_proof_path(wxCommandEvent&)  // [LoggingDialog.md#on_pick_backup_proof_path]
{
    wxString start_dir = backup_proof_path_box_->GetValue();
    if (start_dir.IsEmpty())
        start_dir = wxGetHomeDir();
    wxDirDialog dialog(this, translation::translate("choose_backup_proof_dir"), start_dir);
    dialog.CentreOnParent();
    if (dialog.ShowModal() == wxID_OK)
        backup_proof_path_box_->SetValue(dialog.GetPath());
}

void LoggingDialog::update_notify_enabling()  // [LoggingDialog.md#update_notify_enabling]
{
    const bool active = email_active_check_->IsChecked();
    email_box_->Enable(active);
    const std::initializer_list<wxWindow*> smtp_fields = { smtp_server_box_, smtp_port_ctrl_,
        smtp_user_box_, smtp_password_box_, smtp_from_box_ };
    for (wxWindow* field : smtp_fields)
        field->Enable(active);
}

LoggingDialog::~LoggingDialog()
{}

bool LoggingDialog::TransferDataToWindow()
{
    log_path_box_->SetValue(Logging::instance().get_log_file_path_name());
    int sel = static_cast<int>(Logging::instance().get_log_level());
    log_level_choice_->SetSelection(sel);
    retention_days_ctrl_->SetValue(Logging::instance().retention_days());
    edit_path_box_->SetValue(Logging::instance().get_editor_path_name());
    backup_proof_path_box_->SetValue(Config::instance().settings().backup_proof_path());
    email_active_check_->SetValue(Logging::instance().email().notify_active());
    email_box_->SetValue(Logging::instance().email().notify_address());
    const SmtpAccount smtp = Logging::instance().email().smtp_account();
    smtp_server_box_->SetValue(smtp.server);
    smtp_port_ctrl_->SetValue(smtp.port);
    smtp_user_box_->SetValue(smtp.user);
    smtp_password_box_->SetValue(smtp.password);
    smtp_from_box_->SetValue(smtp.from);
    update_notify_enabling();
    return true;
}

bool LoggingDialog::TransferDataFromWindow()
{
    bool changed = false;
    auto& li = Logging::instance();
	if (li.get_log_file_path_name() != log_path_box_->GetValue().ToStdWstring()) {
		li.set_log_file_path_name(log_path_box_->GetValue().ToStdWstring());
		changed = true;
	}
	if (li.get_log_level() != static_cast<LogLevel>(log_level_choice_->GetSelection())) {
		li.set_log_level(static_cast<LogLevel>(log_level_choice_->GetSelection()));
		changed = true;
	}
	auto last_rd = li.retention_days();
	if (li.retention_days() != retention_days_ctrl_->GetValue()) {
		li.retention_days(retention_days_ctrl_->GetValue());
		if (last_rd > retention_days_ctrl_->GetValue()) {
			LogCut log_cut;
			log_cut();
		}
		changed = true;
	}
    if (li.get_editor_path_name() != edit_path_box_->GetValue().ToStdWstring()) {
        li.set_editor_path_name(edit_path_box_->GetValue().ToStdWstring());
        changed = true;
    }
    Settings& settings = Config::instance().settings();  // [LoggingDialog.md#proof-path-not-in-logging]
    if (settings.backup_proof_path() != backup_proof_path_box_->GetValue().ToStdWstring()) {
        settings.backup_proof_path(backup_proof_path_box_->GetValue().ToStdWstring());
        changed = true;
    }
    if (li.email().notify_active() != email_active_check_->IsChecked()) {
        li.email().notify_active(email_active_check_->IsChecked());
        changed = true;
    }
    if (li.email().notify_address() != email_box_->GetValue().ToStdWstring()) {
        li.email().notify_address(email_box_->GetValue().ToStdWstring());
        changed = true;
    }
    SmtpAccount smtp;  // [LoggingDialog.md#smtp-compared-as-whole]
    smtp.server = smtp_server_box_->GetValue().Trim().Trim(false).ToStdWstring();
    smtp.port = static_cast<uint16_t>(smtp_port_ctrl_->GetValue());
    smtp.user = smtp_user_box_->GetValue().ToStdWstring();
    smtp.password = smtp_password_box_->GetValue().ToStdWstring();
    smtp.from = smtp_from_box_->GetValue().Trim().Trim(false).ToStdWstring();
    if (li.email().smtp_account() != smtp) {
        li.email().smtp_account(smtp);
        changed = true;
    }
    if (changed)
        Config::instance().set_changed();
    return true;
}

bool LoggingDialog::Validate()  // [LoggingDialog.md#Validate]
{
    if (email_active_check_->IsChecked() && email_box_->GetValue().Trim().IsEmpty()) {
        wxMessageBox(translation::translate("email_address_required"), translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        return false;
    }
    if (email_active_check_->IsChecked()  // [LoggingDialog.md#server-required]
        && (smtp_server_box_->GetValue().Trim().IsEmpty() || smtp_from_box_->GetValue().Trim().IsEmpty())) {
        wxMessageBox(translation::translate("smtp_server_required"), translation::translate("validation_error"), wxOK | wxICON_WARNING, this);
        return false;
    }
    return true;
}
