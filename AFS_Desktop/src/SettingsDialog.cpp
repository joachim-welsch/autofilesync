#include "SettingsDialog.h"
#include "config.h"
#include "RecycleBin.h"
#include <wx/dirdlg.h>

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

    // Create auto start field
    wxBoxSizer* startAutostartSizer = new wxBoxSizer(wxHORIZONTAL);
    autoStartCB_ = new wxCheckBox(this, wxID_ANY, "Anwendung automatisch starten");
    startAutostartSizer->Add(autoStartCB_, 1);
    mainSizer->Add(startAutostartSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create start sync at start field
    wxBoxSizer* startSyncAtStartSizer = new wxBoxSizer(wxHORIZONTAL);
    startSyncAtStartCB_ = new wxCheckBox(this, wxID_ANY, "Synchronisation bei Programmstart starten");
    startSyncAtStartSizer->Add(startSyncAtStartCB_, 1);
    mainSizer->Add(startSyncAtStartSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create start minimized field
    wxBoxSizer* startMinimizedSizer = new wxBoxSizer(wxHORIZONTAL);
    startMinimizedCB_ = new wxCheckBox(this, wxID_ANY, "Start minimiert");
    startMinimizedSizer->Add(startMinimizedCB_, 1);
    mainSizer->Add(startMinimizedSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create minimize instead of close field
    wxBoxSizer* minimizeInsteadOfCloseSizer = new wxBoxSizer(wxHORIZONTAL);
    minimizeInsteadOfCloseCB_ = new wxCheckBox(this, wxID_ANY, L"Minimieren statt schließen?");
    minimizeInsteadOfCloseSizer->Add(minimizeInsteadOfCloseCB_, 1);
    mainSizer->Add(minimizeInsteadOfCloseSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create minimize instead of close field
    
    wxBoxSizer* showBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* showBoxLable = new wxStaticText(this, wxID_STATIC, _("Meldungen im Dialog anzeigen:"));
    mainSizer->Add(showBoxSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 5);
    showBoxSizer->Add(showBoxLable, 1);
    wxBoxSizer* showMessageBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    messageBoxErrorCB_ = new wxCheckBox(this, wxID_ANY, "Fehler");
    showMessageBoxSizer->Add(messageBoxErrorCB_);
    messageBoxWarningCB_ = new wxCheckBox(this, wxID_ANY, "Warnung");
    showMessageBoxSizer->Add(messageBoxWarningCB_);
    messageBoxInfoCB_ = new wxCheckBox(this, wxID_ANY, "Info");
    showMessageBoxSizer->Add(messageBoxInfoCB_);
    mainSizer->Add(showMessageBoxSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create delete mode field
    const wxString deleteModes[] = { "In Papierkorb verschieben", L"Endgültig löschen" };
    deleteModeRB_ = new wxRadioBox(this, ID_DELETE_MODE_RADIO, _(L"Löschmodus"), wxDefaultPosition, wxDefaultSize,
        WXSIZEOF(deleteModes), deleteModes, 1, wxRA_SPECIFY_COLS);
    deleteModeRB_->Bind(wxEVT_RADIOBOX, &SettingsDialog::onDeleteModeChanged, this);
    mainSizer->Add(deleteModeRB_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create recycle bin path field
    wxBoxSizer* recycleBinSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* recycleBinLable = new wxStaticText(this, wxID_STATIC, _("Papierkorb:"));
    recycleBinSizer->Add(recycleBinLable, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    recycleBinPathBox_ = new wxTextCtrl(this, ID_RECYCLEBIN_PATH_BOX);
    recycleBinPathBox_->SetMinSize(wxSize(250, recycleBinPathBox_->GetMinSize().y));
    recycleBinSizer->Add(recycleBinPathBox_, 1);
    recycleBinPathBtn_ = new wxButton(this, ID_RECYCLEBIN_PATH_BUTTON, "...", wxDefaultPosition, wxSize(40, -1));
    recycleBinSizer->Add(recycleBinPathBtn_, wxSizerFlags().Border(wxLEFT, 2));
    recycleBinPathBtn_->Bind(wxEVT_BUTTON, &SettingsDialog::onPickRecycleBinPath, this);
    mainSizer->Add(recycleBinSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    //Create button field
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
    startMinimizedCB_->SetValue(s.startMinimized());
    minimizeInsteadOfCloseCB_->SetValue(s.minimizeInsteadOfClose());
    autoStartCB_->SetValue(s.autoStart());
    startSyncAtStartCB_->SetValue(s.startSyncAtStart());
    messageBoxErrorCB_->SetValue(s.showErrors());
    messageBoxWarningCB_->SetValue(s.showWarnings());
    messageBoxInfoCB_->SetValue(s.showInfos());
    recycleBinPathBox_->SetValue(RecycleBin::instance().binPath().wstring());
    deleteModeRB_->SetSelection(s.deleteMode() == DeleteMode::Permanent ? 1 : 0);
    enableRecycleBinPath();
    return true;
}

// The path has no meaning while files are deleted permanently; showing it as
// editable would suggest otherwise.
void SettingsDialog::enableRecycleBinPath()
{
    const bool usesBin = deleteModeRB_->GetSelection() == 0;
    recycleBinPathBox_->Enable(usesBin);
    recycleBinPathBtn_->Enable(usesBin);
}

void SettingsDialog::onDeleteModeChanged(wxCommandEvent&)
{
    enableRecycleBinPath();
}

// The directory need not exist yet: RecycleBin::addItem() creates it on demand,
// so an empty entry falls back to the bin's default path instead of failing.
void SettingsDialog::onPickRecycleBinPath(wxCommandEvent&)
{
    wxString start = recycleBinPathBox_->GetValue();
    if (start.empty() || !wxDirExists(start))
        start = wxGetHomeDir();
    wxDirDialog dialog(this, L"Papierkorb-Verzeichnis wählen", start, wxDD_DEFAULT_STYLE);
    if (dialog.ShowModal() == wxID_OK)
        recycleBinPathBox_->SetValue(dialog.GetPath());
}

bool SettingsDialog::TransferDataFromWindow()
{
    bool changed = false;
    Settings& s = Config::instance().settings();
    if (s.startMinimized() != startMinimizedCB_->GetValue()) {
        s.startMinimized(startMinimizedCB_->GetValue());
        changed = true;
    }
    if (s.minimizeInsteadOfClose() != minimizeInsteadOfCloseCB_->GetValue()) {
        s.minimizeInsteadOfClose(minimizeInsteadOfCloseCB_->GetValue());
        changed = true;
    }
    if (s.autoStart() != autoStartCB_->GetValue()) {
        s.autoStart(autoStartCB_->GetValue());
        changed = true;
    }
    if (s.startSyncAtStart() != startSyncAtStartCB_->GetValue()) {
        s.startSyncAtStart(startSyncAtStartCB_->GetValue());
        changed = true;
    }
    if (s.showErrors() != messageBoxErrorCB_->GetValue()) {
        s.showErrors(messageBoxErrorCB_->GetValue());
        changed = true;
    }
    if (s.showWarnings() != messageBoxWarningCB_->GetValue()) {
        s.showWarnings(messageBoxWarningCB_->GetValue());
        changed = true;
    }
    if (s.showInfos() != messageBoxInfoCB_->GetValue()) {
        s.showInfos(messageBoxInfoCB_->GetValue());
        changed = true;
    }
    const DeleteMode deleteMode = deleteModeRB_->GetSelection() == 1 ? DeleteMode::Permanent : DeleteMode::RecycleBin;
    if (s.deleteMode() != deleteMode) {
        s.deleteMode(deleteMode);
        changed = true;
    }
    const std::wstring recycleBinPath = recycleBinPathBox_->GetValue().ToStdWstring();
    if (!recycleBinPath.empty() && RecycleBin::instance().binPath() != std::filesystem::path(recycleBinPath)) {
        RecycleBin::instance().binPath(recycleBinPath);
        changed = true;
    }
    if (changed)
        Config::instance().setChanged();
    return true;
}

