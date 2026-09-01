#include "SshAccountsDialog.h"
#include "SshAccountDialog.h"
#include "SshAccount.h"
#include "config.h"
#include "TextResources.h"

BEGIN_EVENT_TABLE(SshAccountsDialog, wxDialog)
    EVT_BUTTON(ID_SSHACCOUNTS_ADD, SshAccountsDialog::onAdd)
    EVT_BUTTON(ID_SSHACCOUNTS_EDIT, SshAccountsDialog::onEdit)
    EVT_BUTTON(ID_SSHACCOUNTS_DELETE, SshAccountsDialog::onDelete)
    EVT_LIST_ITEM_SELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onListSelect)
    EVT_LIST_ITEM_DESELECTED(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onListDeselect)
    EVT_LIST_END_LABEL_EDIT(ID_SSHACCOUNTS_LIST, SshAccountsDialog::onEndLabelEdit)
END_EVENT_TABLE()

SshAccountsDialog::SshAccountsDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create list control with editable labels
    listCtrl_ = new wxListCtrl(this, ID_SSHACCOUNTS_LIST, wxDefaultPosition, 
        wxSize(500, 200), wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL);
    
    // Add columns
    listCtrl_->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 120);
    listCtrl_->InsertColumn(1, _("Host"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(2, _("Benutzer"), wxLIST_FORMAT_LEFT, 100);
    listCtrl_->InsertColumn(3, _("Passwort"), wxLIST_FORMAT_LEFT, 100);
    
    mainSizer->Add(listCtrl_, 1, wxEXPAND | wxALL, 10);

    // Create button panel
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    addButton_ = new wxButton(this, ID_SSHACCOUNTS_ADD, _(L"Hinzufügen"));
    editButton_ = new wxButton(this, ID_SSHACCOUNTS_EDIT, _("Bearbeiten..."));
    deleteButton_ = new wxButton(this, ID_SSHACCOUNTS_DELETE, _(L"Löschen"));
    
    buttonSizer->Add(addButton_, 0, wxRIGHT, 5);
    buttonSizer->Add(editButton_, 0, wxRIGHT, 5);
    buttonSizer->Add(deleteButton_, 0, wxRIGHT, 5);
    buttonSizer->AddStretchSpacer();
    
    // OK/Cancel buttons
    wxButton* okButton = new wxButton(this, wxID_OK, _("OK"));
    wxButton* cancelButton = new wxButton(this, wxID_CANCEL, _("Abbrechen"));
    buttonSizer->Add(okButton, 0, wxRIGHT, 5);
    buttonSizer->Add(cancelButton, 0);
    
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(mainSizer);
    SetMinSize(wxSize(550, 300));
    Fit();
    Centre();
    
    // Bind double-click event
    listCtrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountsDialog::onListDClick, this);
    
    populateList();
    updateButtons();
}

SshAccountsDialog::~SshAccountsDialog()
{}

void SshAccountsDialog::populateList()
{
    listCtrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().sshAccountsSnapshot();
    long index = 0;
    for (const auto& account : accounts) {
        // FromUTF8: die Konfiguration haelt diese Felder als UTF-8, die
        // implizite Konvertierung wuerde sie in der ANSI-Codepage lesen.
        long item = listCtrl_->InsertItem(index, wxString::FromUTF8(account.name()));
        listCtrl_->SetItem(item, 1, wxString::FromUTF8(account.host()));
        listCtrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        // Show password as asterisks for security
        listCtrl_->SetItem(item, 3, wxString(account.password().empty() ? "" : "********"));
        index++;
    }
}

void SshAccountsDialog::updateButtons()
{
    bool hasSelection = getSelectedIndex() != -1;
    editButton_->Enable(hasSelection);
    deleteButton_->Enable(hasSelection);
}

long SshAccountsDialog::getSelectedIndex() const
{
    return listCtrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void SshAccountsDialog::onAdd(wxCommandEvent&)
{
    SshAccount newAccount;
    SshAccountDialog dlg(&newAccount, this, wxID_ANY, txt::AddSshConnection);
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(newAccount);
        Config::instance().add(newAccount);
        populateList();
    }
}

void SshAccountsDialog::onEdit(wxCommandEvent&)
{
    editSelectedItem();
}

void SshAccountsDialog::onDelete(wxCommandEvent&)
{
    long sel = getSelectedIndex();
    if (sel == -1)
        return;

    auto account = Config::instance().sshAccountAt(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    wxString message = wxString::Format(_(L"Soll der SSH-Account '%s' wirklich gelöscht werden?"),
        wxString::FromUTF8(account->name()));
    // Unlike a rename, a deletion has no valid account left to point items at
    // - the best this dialog can do is warn instead of letting them silently
    // start failing every sync pass.
    if (Config::instance().sshAccountReferencedByAnyItem(account->name()))
        message += _("\n\nAchtung: Es gibt Synchronisationen, die diesen Account verwenden. Diese werden danach fehlschlagen.");

    int answer = wxMessageBox(message, _(L"Löschen bestätigen"), wxYES_NO | wxICON_QUESTION, this);

    if (answer == wxYES) {
        Config::instance().removeSshAccount(static_cast<size_t>(sel));
        populateList();
        updateButtons();
    }
}

void SshAccountsDialog::onListDClick(wxMouseEvent&)
{
    editSelectedItem();
}

void SshAccountsDialog::onListSelect(wxListEvent&)
{
    updateButtons();
}

void SshAccountsDialog::onListDeselect(wxListEvent&)
{
    updateButtons();
}

void SshAccountsDialog::onEndLabelEdit(wxListEvent& event)
{
    if (event.IsEditCancelled())
        return;

    long sel = event.GetIndex();
    auto account = Config::instance().sshAccountAt(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    if (auto err = validateSshAccountName(event.GetLabel())) {
        wxMessageBox(*err, _("Validierungsfehler"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }

    const std::string oldName = account->name();
    const std::string newName = std::string(event.GetLabel().ToUTF8());
    if (newName != oldName && Config::instance().findSshAccountByName(newName) != std::nullopt) {
        wxMessageBox(wxString::Format(_("SSH-Account '%s' existiert bereits."), event.GetLabel()),
            _("Validierungsfehler"), wxOK | wxICON_WARNING, this);
        event.Veto();
        return;
    }
    account->setName(newName);
    Config::instance().updateSshAccount(static_cast<size_t>(sel), *account);
    // Keeps sync items using this account under its old name from silently
    // failing every pass afterward (see Config::updateSshAccountReferences).
    Config::instance().updateSshAccountReferences(oldName, *account);
}

void SshAccountsDialog::editSelectedItem()
{
    long sel = getSelectedIndex();
    if (sel == -1)
        return;

    auto account = Config::instance().sshAccountAt(static_cast<size_t>(sel));
    if (!account.has_value())
        return;

    const std::string oldName = account->name();
    SshAccountDialog dlg(&account.value(), this, wxID_ANY, txt::EditSshConnection);
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(account.value());
        Config::instance().updateSshAccount(static_cast<size_t>(sel), account.value());
        // Keeps sync items using this account under its old name/credentials
        // from silently failing every pass afterward.
        Config::instance().updateSshAccountReferences(oldName, account.value());
        populateList();
    }
}
