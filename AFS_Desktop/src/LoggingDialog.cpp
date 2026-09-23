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
#include <wx/dirdlg.h>

using namespace AFS_Core;

using util::wstring_to_string;

BEGIN_EVENT_TABLE(LoggingDialog, wxDialog)
    EVT_CHECKBOX(ID_EMAIL_ACTIVE_CHECK, LoggingDialog::onNotifyActiveChanged)
    EVT_BUTTON(ID_BTN_PICK_BACKUP_PROOF_PATH, LoggingDialog::onPickBackupProofPath)
END_EVENT_TABLE()

const wxString choices[] = { "Debug", "Info", "Warning", "Error", "FatalError" };  // [LoggingDialog.md#choices]
long styleLbox = wxLB_SINGLE;

LoggingDialog::LoggingDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);
    notebook->AddPage(createLoggingPage(notebook), _("Logging"), true);
    notebook->AddPage(createEmailPage(notebook), _("E-Mail"));
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(300, 100));
    Fit();
    Centre();
}

wxPanel* LoggingDialog::createLoggingPage(wxNotebook* notebook)
{
    wxPanel* page = new wxPanel(notebook);
    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->AddStretchSpacer()->SetMinSize(300, 20);

    const int labelWidth = page->GetTextExtent(_(L"Pfad für Sicherungsnachweise:")).x + 5;  // [LoggingDialog.md#labelWidth]

    wxBoxSizer* logPathSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* nameLable = new wxStaticText(page, wxID_STATIC, _("Protokolldatei-Pfad:"));
    nameLable->SetMinSize(wxSize(labelWidth, nameLable->GetMinSize().y));
    logPathSizer->Add(nameLable, 0, wxTOP, 3);
    logPathBox_ = new wxTextCtrl(page, ID_LOG_PATH_BOX);
    logPathBox_->SetMinSize(wxSize(250, logPathBox_->GetMinSize().y));
    logPathSizer->Add(logPathBox_);
    pageSizer->Add(logPathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* doubleSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* levelLable = new wxStaticText(page, wxID_STATIC, _("Log Level:"));
    levelLable->SetMinSize(wxSize(labelWidth, levelLable->GetMinSize().y));
    doubleSizer->Add(levelLable, 0, wxEXPAND | wxLEFT, 0);
    logLevelListbox_ =  new wxListBox(page, ID_LOG_LEVEL_LISTBOX, wxDefaultPosition, wxDefaultSize, WXSIZEOF(choices), choices,  styleLbox);
    doubleSizer->Add(logLevelListbox_);

    retentionDaysCtrl_ = new wxSpinCtrl(page, ID_RETENTIONDAYS_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 60, 1);
    wxBoxSizer* retLableSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* retLableTextCtrl = new wxStaticText(page, wxID_STATIC, L"Historie [Tage]:");
    retLableSizer->Add(retLableTextCtrl, 0, wxEXPAND | wxTOP, 3);
    doubleSizer->Add(retLableSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);;
    doubleSizer->Add(retentionDaysCtrl_);
    pageSizer->Add(doubleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* editPathSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* editLable = new wxStaticText(page, wxID_STATIC, _("Editor:"));
    editLable->SetMinSize(wxSize(labelWidth, editLable->GetMinSize().y));
    editPathSizer->Add(editLable, 0, wxTOP, 3);
    editPathBox_ = new wxTextCtrl(page, ID_EDIT_PATH_BOX);
    editPathBox_->SetMinSize(wxSize(250, editPathBox_->GetMinSize().y));
    editPathSizer->Add(editPathBox_);
    pageSizer->Add(editPathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    wxBoxSizer* backupProofPathSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* backupProofLable = new wxStaticText(page, wxID_STATIC, _(L"Pfad für Sicherungsnachweise:"));
    backupProofLable->SetMinSize(wxSize(labelWidth, backupProofLable->GetMinSize().y));
    backupProofPathSizer->Add(backupProofLable, 0, wxTOP, 3);
    backupProofPathBox_ = new wxTextCtrl(page, ID_BACKUP_PROOF_PATH_BOX);
    backupProofPathBox_->SetMinSize(wxSize(250, backupProofPathBox_->GetMinSize().y));
    backupProofPathSizer->Add(backupProofPathBox_);
    backupProofPathSizer->Add(new wxButton(page, ID_BTN_PICK_BACKUP_PROOF_PATH, "...",
        wxDefaultPosition, wxSize(40, -1)), wxSizerFlags().Border(wxLEFT, 2));
    pageSizer->Add(backupProofPathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    page->SetSizer(pageSizer);
    return page;
}

void LoggingDialog::addField(wxPanel* page, wxSizer* pageSizer, const wxString& label,
    wxWindowID id, wxTextCtrl*& box, long style)
{
    wxBoxSizer* fieldSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* text = new wxStaticText(page, wxID_STATIC, label);
    text->SetMinSize(wxSize(110, text->GetMinSize().y));
    fieldSizer->Add(text, 0, wxTOP, 3);
    box = new wxTextCtrl(page, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, style);
    box->SetMinSize(wxSize(250, box->GetMinSize().y));
    fieldSizer->Add(box);
    pageSizer->Add(fieldSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
}

wxPanel* LoggingDialog::createEmailPage(wxNotebook* notebook)
{
    wxPanel* page = new wxPanel(notebook);
    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->AddStretchSpacer()->SetMinSize(300, 20);
    emailActiveCheck_ = new wxCheckBox(page, ID_EMAIL_ACTIVE_CHECK, _("Aktiv"));
    pageSizer->Add(emailActiveCheck_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    addField(page, pageSizer, _("E-Mail-Adresse:"), ID_EMAIL_BOX, emailBox_);

    pageSizer->Add(new wxStaticText(page, wxID_STATIC, _("Postausgangsserver:")),
        0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    addField(page, pageSizer, _("Server:"), ID_SMTP_SERVER_BOX, smtpServerBox_);

    wxBoxSizer* portSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* portLabel = new wxStaticText(page, wxID_STATIC, _("Port:"));
    portLabel->SetMinSize(wxSize(110, portLabel->GetMinSize().y));
    portSizer->Add(portLabel, 0, wxTOP, 3);
    smtpPortCtrl_ = new wxSpinCtrl(page, ID_SMTP_PORT_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 65535, 587);
    portSizer->Add(smtpPortCtrl_);
    pageSizer->Add(portSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    addField(page, pageSizer, _("Benutzer:"), ID_SMTP_USER_BOX, smtpUserBox_);
    addField(page, pageSizer, _("Passwort:"), ID_SMTP_PASSWORD_BOX, smtpPasswordBox_, wxTE_PASSWORD);
    addField(page, pageSizer, _("Absender:"), ID_SMTP_FROM_BOX, smtpFromBox_);

    page->SetSizer(pageSizer);
    return page;
}

void LoggingDialog::onNotifyActiveChanged(wxCommandEvent&)
{
    updateNotifyEnabling();
}

void LoggingDialog::onPickBackupProofPath(wxCommandEvent&)  // [LoggingDialog.md#onPickBackupProofPath]
{
    wxString startDir = backupProofPathBox_->GetValue();
    if (startDir.IsEmpty())
        startDir = wxGetHomeDir();
    wxDirDialog dialog(this, _(L"Verzeichnis für Sicherungsnachweise wählen"), startDir);
    dialog.CentreOnParent();
    if (dialog.ShowModal() == wxID_OK)
        backupProofPathBox_->SetValue(dialog.GetPath());
}

void LoggingDialog::updateNotifyEnabling()  // [LoggingDialog.md#updateNotifyEnabling]
{
    const bool active = emailActiveCheck_->IsChecked();
    emailBox_->Enable(active);
    const std::initializer_list<wxWindow*> smtpFields = { smtpServerBox_, smtpPortCtrl_,
        smtpUserBox_, smtpPasswordBox_, smtpFromBox_ };
    for (wxWindow* field : smtpFields)
        field->Enable(active);
}

LoggingDialog::~LoggingDialog()
{}

bool LoggingDialog::TransferDataToWindow()
{
    logPathBox_->SetValue(Logging::instance().get_log_file_path_name());
    int sel = static_cast<int>(Logging::instance().get_log_level());
    logLevelListbox_->SetSelection(sel);
    retentionDaysCtrl_->SetValue(Logging::instance().retention_days());
    editPathBox_->SetValue(Logging::instance().get_editor_path_name());
    backupProofPathBox_->SetValue(Config::instance().settings().backup_proof_path());
    emailActiveCheck_->SetValue(Logging::instance().notify_email_active());
    emailBox_->SetValue(Logging::instance().notify_email());
    const SmtpAccount smtp = Logging::instance().smtp_account();
    smtpServerBox_->SetValue(smtp.server);
    smtpPortCtrl_->SetValue(smtp.port);
    smtpUserBox_->SetValue(smtp.user);
    smtpPasswordBox_->SetValue(smtp.password);
    smtpFromBox_->SetValue(smtp.from);
    updateNotifyEnabling();
    return true;
}

bool LoggingDialog::TransferDataFromWindow()
{
    bool changed = false;
    auto& li = Logging::instance();
	if (li.get_log_file_path_name() != logPathBox_->GetValue().ToStdWstring()) {
		li.set_log_file_path_name(logPathBox_->GetValue().ToStdWstring());
		changed = true;
	}
	if (li.get_log_level() != static_cast<LogLevel>(logLevelListbox_->GetSelection())) {
		li.set_log_level(static_cast<LogLevel>(logLevelListbox_->GetSelection()));
		changed = true;
	}
	auto lastRD = li.retention_days();
	if (li.retention_days() != retentionDaysCtrl_->GetValue()) {
		li.retention_days(retentionDaysCtrl_->GetValue());
		if (lastRD > retentionDaysCtrl_->GetValue()) {
			LogCut logCut;
			logCut();
		}
		changed = true;
	}
    if (li.get_editor_path_name() != editPathBox_->GetValue().ToStdWstring()) {
        li.set_editor_path_name(editPathBox_->GetValue().ToStdWstring());
        changed = true;
    }
    Settings& settings = Config::instance().settings();  // [LoggingDialog.md#proof-path-not-in-logging]
    if (settings.backup_proof_path() != backupProofPathBox_->GetValue().ToStdWstring()) {
        settings.backup_proof_path(backupProofPathBox_->GetValue().ToStdWstring());
        changed = true;
    }
    if (li.notify_email_active() != emailActiveCheck_->IsChecked()) {
        li.notify_email_active(emailActiveCheck_->IsChecked());
        changed = true;
    }
    if (li.notify_email() != emailBox_->GetValue().ToStdWstring()) {
        li.notify_email(emailBox_->GetValue().ToStdWstring());
        changed = true;
    }
    SmtpAccount smtp;  // [LoggingDialog.md#smtp-compared-as-whole]
    smtp.server = smtpServerBox_->GetValue().Trim().Trim(false).ToStdWstring();
    smtp.port = static_cast<uint16_t>(smtpPortCtrl_->GetValue());
    smtp.user = smtpUserBox_->GetValue().ToStdWstring();
    smtp.password = smtpPasswordBox_->GetValue().ToStdWstring();
    smtp.from = smtpFromBox_->GetValue().Trim().Trim(false).ToStdWstring();
    if (li.smtp_account() != smtp) {
        li.smtp_account(smtp);
        changed = true;
    }
    if (changed)
        Config::instance().set_changed();
    return true;
}

bool LoggingDialog::Validate()  // [LoggingDialog.md#Validate]
{
    if (emailActiveCheck_->IsChecked() && emailBox_->GetValue().Trim().IsEmpty()) {
        wxMessageBox(_("Bitte eine E-Mail-Adresse eingeben oder die Benachrichtigung deaktivieren."), _("E-Mail"));
        return false;
    }
    if (emailActiveCheck_->IsChecked()  // [LoggingDialog.md#server-required]
        && (smtpServerBox_->GetValue().Trim().IsEmpty() || smtpFromBox_->GetValue().Trim().IsEmpty())) {
        wxMessageBox(_("Bitte Postausgangsserver und Absender eintragen."), _("E-Mail"));
        return false;
    }
    return true;
}
