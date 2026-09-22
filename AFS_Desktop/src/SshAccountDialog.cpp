#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "Config.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshAccountDialog, wxDialog)
    EVT_UPDATE_UI(wxID_OK, SshAccountDialog::onUpdateOkButton)
END_EVENT_TABLE()

SshAccountDialog::SshAccountDialog(SshAccount* sshAccount, wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->AddStretchSpacer()->SetMinSize(400, 10);

    wxBoxSizer* nameSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* nameLabel = new wxStaticText(this, wxID_STATIC, _("Name"));
    nameLabel->SetMinSize(wxSize(80, nameLabel->GetMinSize().y));
    nameSizer->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL);
    nameBox_ = new wxTextCtrl(this, ID_SSH_NAME_BOX);
    nameSizer->Add(nameBox_, 1);
    mainSizer->Add(nameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);

    wxBoxSizer* hostSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* hostLabel = new wxStaticText(this, wxID_STATIC, _("Host"));
    hostLabel->SetMinSize(wxSize(80, hostLabel->GetMinSize().y));
    hostSizer->Add(hostLabel, 0, wxALIGN_CENTER_VERTICAL);
    hostBox_ = new wxTextCtrl(this, ID_SSH_HOST_BOX);
    hostSizer->Add(hostBox_, 1);
    mainSizer->Add(hostSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* userSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* userLabel = new wxStaticText(this, wxID_STATIC, _("Benutzer"));
    userLabel->SetMinSize(wxSize(80, userLabel->GetMinSize().y));
    userSizer->Add(userLabel, 0, wxALIGN_CENTER_VERTICAL);
    userBox_ = new wxTextCtrl(this, ID_SSH_USER_BOX);
    userSizer->Add(userBox_, 1);
    mainSizer->Add(userSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxBoxSizer* passwordSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* passwordLabel = new wxStaticText(this, wxID_STATIC, _("Passwort"));
    passwordLabel->SetMinSize(wxSize(80, passwordLabel->GetMinSize().y));
    passwordSizer->Add(passwordLabel, 0, wxALIGN_CENTER_VERTICAL);
    passwordBox_ = new wxTextCtrl(this, ID_SSH_PASSWORD_BOX, wxEmptyString, 
        wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    passwordSizer->Add(passwordBox_, 1);
    mainSizer->Add(passwordSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(400, 180));
    Fit();
    Centre();

    if (sshAccount != nullptr)
        setData(*sshAccount);
}

SshAccountDialog::~SshAccountDialog()
{}

static wxString passwordFromAccount(const std::string& stored)  // [SshAccountDialog.md#passwordFromAccount]
{
    const wxString password = wxString::FromUTF8(stored);
    return password.IsEmpty() && !stored.empty() ? wxString::FromUTF8(stored) : password;
}

void SshAccountDialog::setData(const SshAccount& account)
{
    name_ = wxString::FromUTF8(account.name());  // [SshAccountDialog.md#utf8-fields]
    user_ = wxString::FromUTF8(account.user());
    host_ = wxString::FromUTF8(account.host());
    password_ = passwordFromAccount(account.password());
}

void SshAccountDialog::saveData(SshAccount& account)
{
    account.set_name(std::string(name_.ToUTF8()));  // [SshAccountDialog.md#utf8-on-the-wire]
    account.set_user(std::string(user_.ToUTF8()));
    account.set_host(std::string(host_.ToUTF8()));
    account.set_password(std::string(password_.ToUTF8()));
}

bool SshAccountDialog::TransferDataToWindow()
{
    nameBox_->SetValue(name_);
    userBox_->SetValue(user_);
    hostBox_->SetValue(host_);
    passwordBox_->SetValue(password_);
    return true;
}

bool SshAccountDialog::TransferDataFromWindow()
{
    name_ = nameBox_->GetValue();
    user_ = userBox_->GetValue();
    host_ = hostBox_->GetValue();
    password_ = passwordBox_->GetValue();
    return true;
}

std::optional<wxString> validateSshAccountName(const wxString& name)
{
    if (name.length() < 2)
        return _("Der Name muss mindestens 2 Zeichen lang sein.");
    if (name.Contains(":"))  // [SshAccountDialog.md#colon-first]
        return _("Der Name darf keinen Doppelpunkt enthalten.");
    for (const wxChar c : name) {
        if (!wxIsalnum(c) && c != '_' && c != '-')
            return _("Der Name darf nur Buchstaben, Zahlen, Unterstrich und Bindestrich enthalten.");
    }
    return std::nullopt;
}

bool SshAccountDialog::Validate()
{
    wxString name = nameBox_->GetValue();

    if (auto err = validateSshAccountName(name)) {
        wxMessageBox(*err, _("Validierungsfehler"), wxOK | wxICON_WARNING, this);
        nameBox_->SetFocus();
        return false;
    }

    if (name != name_ && Config::instance().find_ssh_account_by_name(std::string(name.ToUTF8())) != std::nullopt) {  // [SshAccountDialog.md#name-compared-against-original]
        wxMessageBox(wxString::Format(_("SSH-Account '%s' existiert bereits."), name), _("Validierungsfehler"),
            wxOK | wxICON_WARNING, this);
        nameBox_->SetFocus();
        return false;
    }

    if (hostBox_->GetValue().IsEmpty()) {
        wxMessageBox(_("Bitte geben Sie einen Host an."), _("Validierungsfehler"), 
            wxOK | wxICON_WARNING, this);
        hostBox_->SetFocus();
        return false;
    }
    if (userBox_->GetValue().IsEmpty()) {
        wxMessageBox(_("Bitte geben Sie einen Benutzer an."), _("Validierungsfehler"), 
            wxOK | wxICON_WARNING, this);
        userBox_->SetFocus();
        return false;
    }
    return true;
}

void SshAccountDialog::onUpdateOkButton(wxUpdateUIEvent& event)
{
    event.Enable(!hostBox_->GetValue().IsEmpty() && !userBox_->GetValue().IsEmpty());
}
