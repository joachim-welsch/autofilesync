#include "SshAccountSelectDialog.h"
#include "config.h"

BEGIN_EVENT_TABLE(SshAccountSelectDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_SSH_SELECT_LIST, SshAccountSelectDialog::onListSelect)
END_EVENT_TABLE()

SshAccountSelectDialog::SshAccountSelectDialog(wxWindow* parent,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxDialog(parent, wxID_ANY, title, pos, size, style)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Info text
    wxStaticText* infoText = new wxStaticText(this, wxID_STATIC,
        _(L"Wählen Sie eine SSH-Verbindung aus:"));
    mainSizer->Add(infoText, 0, wxEXPAND | wxALL, 10);

    // List control
    listCtrl_ = new wxListCtrl(this, ID_SSH_SELECT_LIST, wxDefaultPosition,
        wxSize(400, 200), wxLC_REPORT | wxLC_SINGLE_SEL);
    
    listCtrl_->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(1, _("Host"), wxLIST_FORMAT_LEFT, 150);
    listCtrl_->InsertColumn(2, _("Benutzer"), wxLIST_FORMAT_LEFT, 100);
    
    mainSizer->Add(listCtrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    // Bind double-click
    listCtrl_->Bind(wxEVT_LEFT_DCLICK, &SshAccountSelectDialog::onListDClick, this);

    // Buttons
    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);

    SetSizer(mainSizer);
    SetMinSize(wxSize(450, 300));
    Fit();
    Centre();

    populateList();
    updateButtons();
}

SshAccountSelectDialog::~SshAccountSelectDialog()
{}

void SshAccountSelectDialog::populateList()
{
    listCtrl_->DeleteAllItems();

    const SshAccounts accounts = Config::instance().sshAccountsSnapshot();
    long index = 0;
    for (const auto& account : accounts) {
        // FromUTF8 - siehe SshAccountsDialog::updateList().
        long item = listCtrl_->InsertItem(index, wxString::FromUTF8(account.name()));
        listCtrl_->SetItem(item, 1, wxString::FromUTF8(account.host()));
        listCtrl_->SetItem(item, 2, wxString::FromUTF8(account.user()));
        index++;
    }
}

void SshAccountSelectDialog::onListDClick(wxMouseEvent&)
{
    if (selectedIndex_ >= 0) {
        EndModal(wxID_OK);
    }
}

void SshAccountSelectDialog::onListSelect(wxListEvent& event)
{
    selectedIndex_ = event.GetIndex();
    updateButtons();
}

void SshAccountSelectDialog::updateButtons()
{
    wxButton* okButton = dynamic_cast<wxButton*>(FindWindow(wxID_OK));
    if (okButton) {
        okButton->Enable(selectedIndex_ >= 0);
    }
}

std::optional<SshAccount> SshAccountSelectDialog::getSelectedAccount() const
{
    if (selectedIndex_ < 0) {
        return std::nullopt;
    }
    return Config::instance().sshAccountAt(static_cast<size_t>(selectedIndex_));
}
