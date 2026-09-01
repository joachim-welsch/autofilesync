#include "LoggingDialog.h"
#include "Logging.h"
#include "id.h"
#include "util.h"
#include "config.h"
#include "LogCut.h"
#include <wx/dirdlg.h>

using util::wstring_to_string;

BEGIN_EVENT_TABLE(LoggingDialog, wxDialog)
    EVT_CHECKBOX(ID_EMAIL_ACTIVE_CHECK, LoggingDialog::onNotifyActiveChanged)
    EVT_BUTTON(ID_BTN_PICK_BACKUP_PROOF_PATH, LoggingDialog::onPickBackupProofPath)
END_EVENT_TABLE()

// Index maps directly onto LogLevel (see the static_cast in
// TransferDataFromWindow), so the order must stay the one of the enum.
const wxString choices[] = { "Debug", "Info", "Warning", "Error", "FatalError" };
// long styleLbox = wxLB_ALWAYS_SB | wxLB_EXTENDED | wxLB_SINGLE;
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

    // --------------------------------------------------------------------------
    //Create button field
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

    // One label column for the whole page, measured from the longest label: a
    // fixed pixel width clips as soon as the font or the DPI differs, and a
    // label only as wide as itself starts every field at a different place.
    const int labelWidth = page->GetTextExtent(_(L"Pfad für Sicherungsnachweise:")).x + 5;

    // Create Log path elements
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
    //logLevelListbox_->SetSelection(0);
    doubleSizer->Add(logLevelListbox_);

    // Create log retention days elements

    retentionDaysCtrl_ = new wxSpinCtrl(page, ID_RETENTIONDAYS_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 60, 1);
    wxBoxSizer* retLableSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* retLableTextCtrl = new wxStaticText(page, wxID_STATIC, L"Historie [Tage]:");
    retLableSizer->Add(retLableTextCtrl, 0, wxEXPAND | wxTOP, 3);
    doubleSizer->Add(retLableSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);;
    doubleSizer->Add(retentionDaysCtrl_);
    pageSizer->Add(doubleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    // Create Log editor path elements
    wxBoxSizer* editPathSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* editLable = new wxStaticText(page, wxID_STATIC, _("Editor:"));
    editLable->SetMinSize(wxSize(labelWidth, editLable->GetMinSize().y));
    editPathSizer->Add(editLable, 0, wxTOP, 3);
    editPathBox_ = new wxTextCtrl(page, ID_EDIT_PATH_BOX);
    editPathBox_->SetMinSize(wxSize(250, editPathBox_->GetMinSize().y));
    editPathSizer->Add(editPathBox_);
    pageSizer->Add(editPathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    // Create backup proof path elements
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

// Offers directories, not files: the setting names the directory the proofs go
// into, one file per sync item. A directory that does not exist yet is allowed
// - writeBackupProof() creates it, and the dialog can create it here as well.
void LoggingDialog::onPickBackupProofPath(wxCommandEvent&)
{
    wxString startDir = backupProofPathBox_->GetValue();
    if (startDir.IsEmpty())
        startDir = wxGetHomeDir();
    wxDirDialog dialog(this, _(L"Verzeichnis für Sicherungsnachweise wählen"), startDir);
    dialog.CentreOnParent();
    if (dialog.ShowModal() == wxID_OK)
        backupProofPathBox_->SetValue(dialog.GetPath());
}

// The address stays readable while switched off - greying it out says "not in
// use" without hiding what was configured.
void LoggingDialog::updateNotifyEnabling()
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
    logPathBox_->SetValue(Logging::instance().getLogFilePathName());
    int sel = static_cast<int>(Logging::instance().getLogLevel());
    logLevelListbox_->SetSelection(sel);
    retentionDaysCtrl_->SetValue(Logging::instance().retentionDays());
    editPathBox_->SetValue(Logging::instance().getEditorPathName());
    backupProofPathBox_->SetValue(Config::instance().settings().backupProofPath());
    emailActiveCheck_->SetValue(Logging::instance().notifyEmailActive());
    emailBox_->SetValue(Logging::instance().notifyEmail());
    const SmtpAccount smtp = Logging::instance().smtpAccount();
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
	if (li.getLogFilePathName() != logPathBox_->GetValue().ToStdWstring()) {
		li.setLogFilePathName(logPathBox_->GetValue().ToStdWstring());
		changed = true;
	}
	if (li.getLogLevel() != static_cast<LogLevel>(logLevelListbox_->GetSelection())) {
		li.setLogLevel(static_cast<LogLevel>(logLevelListbox_->GetSelection()));
		changed = true;
	}
	auto lastRD = li.retentionDays();
	if (li.retentionDays() != retentionDaysCtrl_->GetValue()) {
		li.retentionDays(retentionDaysCtrl_->GetValue());
		if (lastRD > retentionDaysCtrl_->GetValue()) {
			LogCut logCut;
			logCut();
		}
		changed = true;
	}
    if (li.getEditorPathName() != editPathBox_->GetValue().ToStdWstring()) {
        li.setEditorPathName(editPathBox_->GetValue().ToStdWstring());
        changed = true;
    }
    // Not part of Logging: the proofs are the application's own artefact, and
    // Logging is the shared library the daemon logs through as well.
    Settings& settings = Config::instance().settings();
    if (settings.backupProofPath() != backupProofPathBox_->GetValue().ToStdWstring()) {
        settings.backupProofPath(backupProofPathBox_->GetValue().ToStdWstring());
        changed = true;
    }
    if (li.notifyEmailActive() != emailActiveCheck_->IsChecked()) {
        li.notifyEmailActive(emailActiveCheck_->IsChecked());
        changed = true;
    }
    if (li.notifyEmail() != emailBox_->GetValue().ToStdWstring()) {
        li.notifyEmail(emailBox_->GetValue().ToStdWstring());
        changed = true;
    }
    // Compared as a whole: the account is stored as one value, so a per-field
    // comparison would only repeat what operator!= already answers.
    SmtpAccount smtp;
    smtp.server = smtpServerBox_->GetValue().Trim().Trim(false).ToStdWstring();
    smtp.port = static_cast<uint16_t>(smtpPortCtrl_->GetValue());
    smtp.user = smtpUserBox_->GetValue().ToStdWstring();
    smtp.password = smtpPasswordBox_->GetValue().ToStdWstring();
    smtp.from = smtpFromBox_->GetValue().Trim().Trim(false).ToStdWstring();
    if (li.smtpAccount() != smtp) {
        li.smtpAccount(smtp);
        changed = true;
    }
    if (changed)
        Config::instance().setChanged();
    return true;
}

// "Aktiv" without a recipient would look like a working notification and
// silently deliver nothing - exactly what one does not want to find out about
// after the error it was meant to report.
bool LoggingDialog::Validate()
{
    if (emailActiveCheck_->IsChecked() && emailBox_->GetValue().Trim().IsEmpty()) {
        wxMessageBox(_("Bitte eine E-Mail-Adresse eingeben oder die Benachrichtigung deaktivieren."), _("E-Mail"));
        return false;
    }
    // The notification goes out over this server; without it the switch above
    // would again promise something that never arrives.
    if (emailActiveCheck_->IsChecked()
        && (smtpServerBox_->GetValue().Trim().IsEmpty() || smtpFromBox_->GetValue().Trim().IsEmpty())) {
        wxMessageBox(_("Bitte Postausgangsserver und Absender eintragen."), _("E-Mail"));
        return false;
    }
    return true;
}
