#include "SyncItemDialog.h"
#include <sstream>
#include <filesystem>
#include <wx/sizer.h>
#include "wx/filedlg.h"
#include <wx/dirdlg.h>
#include "SyncItem.h"
#include "Config.h"
#include "Util.h"
#include "Logging.h"
#include "SshAccountSelectDialog.h"
#include "SshDirDialog.h"

using namespace AFS_Core;

namespace fs = std::filesystem;
using util::wstring_to_string;
using util::string_to_wstring;
using std::wstring;

BEGIN_EVENT_TABLE(SyncItemDialog, wxDialog)
    EVT_UPDATE_UI(wxID_OK, SyncItemDialog::onUpdateOkButton)
END_EVENT_TABLE()

SyncItemDialog::SyncItemDialog(SyncItem* syncItem, wxWindow* parent, wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style ,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create notebook with two tabs
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    // ==========================================================================
    // Tab 1: Allgemein (Name, Typ, Wildcard, Quelle, Ziel)
    // ==========================================================================
    wxPanel* tab1 = new wxPanel(notebook);
    wxBoxSizer* tab1Sizer = new wxBoxSizer(wxVERTICAL);
    tab1Sizer->AddStretchSpacer()->SetMinSize(720, 10);

    // Create name elements
    wxBoxSizer* nameSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* nameLable = new wxStaticText(tab1, wxID_STATIC, _("Name"));
    nameLable->SetMinSize(wxSize(60, nameLable->GetMinSize().y));
    nameSizer->Add(nameLable, 0, wxALIGN_CENTER_VERTICAL);
    nameBox_ = new wxTextCtrl(tab1, ID_NAME_BOX);
    nameSizer->Add(nameBox_, 1);
    tab1Sizer->Add(nameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create Type field
    wxBoxSizer* typeSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* typeLable = new wxStaticText(tab1, wxID_STATIC, _("Typ"));
    typeLable->SetMinSize(wxSize(60, typeLable->GetMinSize().y));
    typeSizer->Add(typeLable, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString types;
    types.push_back(sync_item_type_to_str(SyncItemType::File));
    types.push_back(sync_item_type_to_str(SyncItemType::Dir));
    types.push_back(sync_item_type_to_str(SyncItemType::Tree));

    typeChoice_ = new wxChoice(tab1, ID_TYPE_CHOICE, wxDefaultPosition, wxDefaultSize, types);
    typeSizer->Add(typeChoice_, 1);

    // Create Wildcard field
    wxStaticText* wildcardLable = new wxStaticText(tab1, wxID_STATIC, _("Wildcard"));
    wildcardLable->SetMinSize(wxSize(75, wildcardLable->GetMinSize().y));
    typeSizer->Add(wildcardLable, wxSizerFlags().Border(wxLEFT).CenterVertical());

    wildcardBox_ = new wxTextCtrl(tab1, ID_WILDCARD_BOX);
    typeSizer->Add(wildcardBox_, wxSizerFlags().Border(wxLEFT));

    tab1Sizer->Add(typeSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create sourceDir field
    wxBoxSizer* sourceDirSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* sourceDirLable = new wxStaticText(tab1, wxID_STATIC, _("Quelle"));
    sourceDirLable->SetMinSize(wxSize(60, sourceDirLable->GetMinSize().y));
    sourceDirSizer->Add(sourceDirLable, 0, wxALIGN_CENTER_VERTICAL);

    sourceDirBox_ = new wxTextCtrl(tab1, ID_SOURCE_DIR_BOX);
    sourceDirSizer->Add(sourceDirBox_, 1);
    wxButton* btnPickSourceDirText = new wxButton(tab1, ID_BTN_PICK_SOURCE_DIR, "...", 
        wxDefaultPosition, wxSize(40, -1));
    sourceDirSizer->Add(btnPickSourceDirText, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btnSshSource = new wxButton(tab1, ID_BTN_SSH_SOURCE, "ssh",
        wxDefaultPosition, wxSize(45, -1));
    sourceDirSizer->Add(btnSshSource, wxSizerFlags().Border(wxLEFT, 2));

    tab1Sizer->Add(sourceDirSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create targetDir field
    wxBoxSizer* targetDirSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* targetDirLable = new wxStaticText(tab1, wxID_STATIC, _("Ziel"));
    targetDirLable->SetMinSize(wxSize(60, targetDirLable->GetMinSize().y));
    targetDirSizer->Add(targetDirLable, 0, wxALIGN_CENTER_VERTICAL);

    targetDirBox_ = new wxTextCtrl(tab1, ID_TARGET_DIR_BOX);
    targetDirSizer->Add(targetDirBox_, 1);

    wxButton* btnPickTargetDirText = new wxButton(tab1, ID_BTN_PICK_TARGET_DIR, "...",
        wxDefaultPosition, wxSize(40, -1));
    targetDirSizer->Add(btnPickTargetDirText, wxSizerFlags().Border(wxLEFT, 2));
    wxButton* btnSshTarget = new wxButton(tab1, ID_BTN_SSH_TARGET, "ssh",
        wxDefaultPosition, wxSize(45, -1));
    targetDirSizer->Add(btnSshTarget, wxSizerFlags().Border(wxLEFT, 2));

    tab1Sizer->Add(targetDirSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    tab1->SetSizer(tab1Sizer);
    notebook->AddPage(tab1, _("Allgemein"));

    // ==========================================================================
    // Tab 2: Optionen (alle Checkboxen und Einstellungen)
    // ==========================================================================
    wxPanel* tab2 = new wxPanel(notebook);
    wxBoxSizer* tab2Sizer = new wxBoxSizer(wxVERTICAL);
    tab2Sizer->AddStretchSpacer()->SetMinSize(720, 10);

    // Create enabled field
    wxBoxSizer* enabledSizer = new wxBoxSizer(wxHORIZONTAL);
    enabledCheck_ = new wxCheckBox(tab2, ID_ENABLED_CHECK, "Synchronisation aktiv");
    enabledSizer->Add(enabledCheck_, 1);
    wxStaticText* compareLable = new wxStaticText(tab2, wxID_STATIC, _("Dateivergleich nach:"));
    enabledSizer->Add(compareLable, 1);
    tab2Sizer->Add(enabledSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create delSourceAfter field
    wxBoxSizer* delSourceAfterSizer = new wxBoxSizer(wxHORIZONTAL);
    delSourceAfterCheck_ = new wxCheckBox(tab2, ID_DEL_SOURCE_AFTER_CHECK, L"Quelle nach dem Kopieren löschen");
    delSourceAfterSizer->Add(delSourceAfterCheck_, 1);
    compareDateCheck_ = new wxCheckBox(tab2, ID_COMPARE_DATE_CHECK, "Zeit");
    delSourceAfterSizer->Add(compareDateCheck_, 1);

    tab2Sizer->Add(delSourceAfterSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create twoWaySync field
    wxBoxSizer* twoWaySyncSizer = new wxBoxSizer(wxHORIZONTAL);
    twoWaySyncCheck_ = new wxCheckBox(tab2, ID_TWO_WAY_SYNC_CHECK, "Zwei-Wege-Synchronisation");
    twoWaySyncSizer->Add(twoWaySyncCheck_, 1);
    compareSizeCheck_ = new wxCheckBox(tab2, ID_COMPARE_SIZE_CHECK, L"Größe");
    twoWaySyncSizer->Add(compareSizeCheck_, 1);

    tab2Sizer->Add(twoWaySyncSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create copyWhenTargetNewer field
    wxBoxSizer* copyWhenTargetNewerSizer = new wxBoxSizer(wxHORIZONTAL);
    copyWhenTargetNewerCheck_ = new wxCheckBox(tab2, ID_CPY_WHEN_TARGET_NEWER_CHECK, "Kopieren, auch wenn Zieldatei neuer ist");
    copyWhenTargetNewerSizer->Add(copyWhenTargetNewerCheck_, 1);
    compareContentCheck_ = new wxCheckBox(tab2, ID_COMPARE_CONTENT_CHECK, "Inhalt");
    copyWhenTargetNewerSizer->Add(compareContentCheck_, 1);

    tab2Sizer->Add(copyWhenTargetNewerSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create fatherSonPrinciple field
    wxBoxSizer* fatherSonPrincipleSizer = new wxBoxSizer(wxHORIZONTAL);
    fatherSonPrincipleCheck_ = new wxCheckBox(tab2, ID_FATHER_SON_PRINCIPLE_CHECK, "Generationenprinzip");
    fatherSonPrincipleSizer->Add(fatherSonPrincipleCheck_, 1);

    tab2Sizer->Add(fatherSonPrincipleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create deleteInTarget field
    wxBoxSizer* deleteInTargetSizer = new wxBoxSizer(wxHORIZONTAL);
    deleteInTargetCheck_ = new wxCheckBox(tab2, ID_DELETE_IN_TARGET_CHECK, L"Gelöschte Dateien im Ziel ebenfalls löschen");
    deleteInTargetSizer->Add(deleteInTargetCheck_, 1);

    tab2Sizer->Add(deleteInTargetSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create backupProof field
    wxBoxSizer* backupProofSizer = new wxBoxSizer(wxHORIZONTAL);
    backupProofCheck_ = new wxCheckBox(tab2, ID_BACKUP_PROOF_CHECK, "Sicherungsnachweis");
    backupProofSizer->Add(backupProofCheck_, 1);

    tab2Sizer->Add(backupProofSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create syncEmptyFiles field
    wxBoxSizer* syncEmptyFilesSizer = new wxBoxSizer(wxHORIZONTAL);
    syncEmptyFilesCheck_ = new wxCheckBox(tab2, ID_SYNC_EMPTY_FILES_CHECK, L"Leere Dateien synchronisieren");
    syncEmptyFilesSizer->Add(syncEmptyFilesCheck_, 1);

    tab2Sizer->Add(syncEmptyFilesSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Create sync cycle control
    wxBoxSizer* syncCycleSizer = new wxBoxSizer(wxHORIZONTAL);
    syncCycleSpin_ = new wxSpinCtrl(tab2, ID_SYNC_CYCLE_SPIN, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 60, 1);
    syncCycleSizer->Add(new wxStaticText(tab2, wxID_STATIC, L"Sekunden Pause:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    syncCycleSizer->Add(syncCycleSpin_, wxSizerFlags().Border(wxRIGHT));
    tab2Sizer->Add(syncCycleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    tab2->SetSizer(tab2Sizer);
    notebook->AddPage(tab2, _("Optionen"));

    // ==========================================================================
    // Tab 3: Ausschlüsse (Dateien/Verzeichnisse, die nie synchronisiert werden)
    // ==========================================================================
    wxPanel* tab3 = new wxPanel(notebook);
    wxBoxSizer* tab3Sizer = new wxBoxSizer(wxVERTICAL);
    tab3Sizer->Add(new wxStaticText(tab3, wxID_STATIC,
        _("Namen oder Muster, die von der Synchronisation ausgeschlossen werden\n"
          "(ein Eintrag pro Zeile, z.B. *.tmp oder node_modules):")),
        0, wxALL, 5);
    exclusionsBox_ = new wxTextCtrl(tab3, ID_EXCLUSIONS_BOX, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    tab3Sizer->Add(exclusionsBox_, 1, wxEXPAND | wxALL, 5);
    tab3->SetSizer(tab3Sizer);
    notebook->AddPage(tab3, _(L"Ausschlüsse"));

    // Add notebook to main sizer
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    // Create button field
    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    btnPickSourceDirText->Bind(wxEVT_BUTTON, &SyncItemDialog::onPickSourceDirText, this);
    btnPickTargetDirText->Bind(wxEVT_BUTTON, &SyncItemDialog::onPickTargetDirText, this);
    btnSshSource->Bind(wxEVT_BUTTON, &SyncItemDialog::onSshSource, this);
    btnSshTarget->Bind(wxEVT_BUTTON, &SyncItemDialog::onSshTarget, this);

    SetSizer(mainSizer);
    SetMinSize(wxSize(300, 100));
    Fit();
    Centre();
    if (syncItem != nullptr)
        setData(*syncItem);
    eventBindings();
}

void SyncItemDialog::updateDeleteInTargetState()
{
    bool isFileType = (typeChoice_->GetSelection() != wxNOT_FOUND) &&
                      (typeChoice_->GetString(typeChoice_->GetSelection()) == sync_item_type_to_str(SyncItemType::File));
    if (isFileType) {
        deleteInTargetCheck_->SetValue(false);
        deleteInTargetCheck_->Enable(false);
    } else {
        deleteInTargetCheck_->Enable(true);
    }
}

void SyncItemDialog::eventBindings()
{
    twoWaySyncCheck_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (twoWaySyncCheck_->GetValue()) {
            copyWhenTargetNewerCheck_->SetValue(false);
            copyWhenTargetNewerCheck_->Enable(false);
        } else {
            copyWhenTargetNewerCheck_->Enable(true);
        }
    });

    typeChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        updateDeleteInTargetState();
    });
}

SyncItemDialog::~SyncItemDialog()
{}

void SyncItemDialog::setData(const SyncItem& si)
{
    wildcard_ = si.wildcard();
    exclusions_.Clear();
    for (const auto& pattern : si.exclusions()) {
        if (!exclusions_.empty())
            exclusions_ += "\n";
        exclusions_ += wxString(pattern);
    }
    source_ = si.source();
    target_ = si.target();
    name_ = si.name();
    type_ = si.item_type_str();
    enabled_ = si.enabled();
    delSourceAfter_ = si.del_aftercopy();
    compareDate_ = si.compare_items().time();
    twoWaySync_ = si.two_way_sync();
    compareSize_ = si.compare_items().size();
    copyWhenTargetNewer_ = si.copy_when_target_newer();
    compareContent_ = si.compare_items().content();
    fatherSonPrinciple_ = si.father_son_principle();
    deleteInTarget_ = si.delete_in_target();
    backupProof_ = si.backup_proof();
    syncEmptyFiles_ = si.sync_empty_files();
    syncCycle_ = si.sync_cycle();
    
}

void SyncItemDialog::saveData(SyncItem& si)
{
    log_info("SyncItemDialog::saveData called");
    si.name(name_.ToStdWstring());
    si.wildcard(wildcard_.ToStdWstring());
    {
        // One wildcard pattern per line; blank/whitespace-only lines are
        // ignored so trailing newlines or accidental extra Enters don't turn
        // into a pattern that matches nothing (or, worse, an empty pattern).
        Exclusions exclusions;
        std::wistringstream lines(exclusions_.ToStdWstring());
        std::wstring line;
        while (std::getline(lines, line)) {
            const auto first = line.find_first_not_of(L" \t\r");
            if (first == std::wstring::npos)
                continue;
            const auto last = line.find_last_not_of(L" \t\r");
            exclusions.push_back(line.substr(first, last - first + 1));
        }
        si.exclusions(exclusions);
    }
    si.source(source_.ToStdWstring());
    si.target(target_.ToStdWstring());
    si.sync_item_type(str_to_sync_item_type(type_.ToStdWstring()));
    si.enabled(enabled_);
    si.del_aftercopy(delSourceAfter_);
    CompareItems compareItems;
    compareItems.time(compareDate_);
    si.two_way_sync(twoWaySync_);
    compareItems.size(compareSize_);
    si.copy_when_target_newer(copyWhenTargetNewer_);
    compareItems.content(compareContent_);
    si.compare_items(compareItems);
    si.father_son_principle(fatherSonPrinciple_);
    si.delete_in_target(deleteInTarget_);
    si.backup_proof(backupProof_);
    si.sync_empty_files(syncEmptyFiles_);
    si.sync_cycle(syncCycle_);
}

void SyncItemDialog::onUpdateOkButton(wxUpdateUIEvent& event)
{
    event.Enable(false);
    // An empty wildcard is a valid, meaningful choice (matches every file -
    // see SyncBase::syncDirEntry()'s "wildcard.empty() || wildmatch(...)"),
    // not an incomplete form; requiring one just blocked OK with no
    // indication why.
    if (!sourceDirBox_->GetValue().empty() && !targetDirBox_->GetValue().empty()) {
        int selection = typeChoice_->GetSelection();
        if (selection != wxNOT_FOUND && (compareSizeCheck_->GetValue() || compareContentCheck_->GetValue() || compareDateCheck_->GetValue()))
            event.Enable(true);
    }
}

bool SyncItemDialog::TransferDataToWindow()
{
    nameBox_->SetValue(name_);
    wildcardBox_->SetValue(wildcard_);
    exclusionsBox_->SetValue(exclusions_);
    sourceDirBox_->SetValue(source_);
    targetDirBox_->SetValue(target_);
    int selection = typeChoice_->FindString(type_);
    if (selection != wxNOT_FOUND)
        typeChoice_->SetSelection(selection);
    enabledCheck_->SetValue(enabled_);
    delSourceAfterCheck_->SetValue(delSourceAfter_);
    compareDateCheck_->SetValue(compareDate_);
    twoWaySyncCheck_->SetValue(twoWaySync_);
    compareSizeCheck_->SetValue(compareSize_);
    copyWhenTargetNewerCheck_->SetValue(copyWhenTargetNewer_);
    compareContentCheck_->SetValue(compareContent_);
    fatherSonPrincipleCheck_->SetValue(fatherSonPrinciple_);
    deleteInTargetCheck_->SetValue(deleteInTarget_);
    backupProofCheck_->SetValue(backupProof_);
    syncEmptyFilesCheck_->SetValue(syncEmptyFiles_);
    syncCycleSpin_->SetValue(syncCycle_);
    updateDeleteInTargetState();
    return true;
}

bool SyncItemDialog::TransferDataFromWindow()
{
    name_ = nameBox_->GetValue();
    wildcard_ = wildcardBox_->GetValue();
    exclusions_ = exclusionsBox_->GetValue();
    source_ = sourceDirBox_->GetValue();
    target_ = targetDirBox_->GetValue();
    int selection = typeChoice_->GetSelection();
    if (selection != wxNOT_FOUND)
        type_ = typeChoice_->GetString(selection);
    else
        type_ = "";
    enabled_ = enabledCheck_->GetValue();
    delSourceAfter_ = delSourceAfterCheck_->GetValue();
    compareDate_ = compareDateCheck_->GetValue();
    twoWaySync_ = twoWaySyncCheck_->GetValue();
    compareSize_ = compareSizeCheck_->GetValue();
    copyWhenTargetNewer_ = copyWhenTargetNewerCheck_->GetValue();
    compareContent_ = compareContentCheck_->GetValue();
    fatherSonPrinciple_ = fatherSonPrincipleCheck_->GetValue();
    deleteInTarget_ = deleteInTargetCheck_->GetValue();
    backupProof_ = backupProofCheck_->GetValue();
    syncEmptyFiles_ = syncEmptyFilesCheck_->GetValue();
    syncCycle_ = syncCycleSpin_->GetValue();
    return true;
}

wxString withVolumenName(const wxString& path)
{
#ifdef _UNIX
    // No volume spec on Unix: mountpoints already identify drives, and a
    // "path (device)" suffix would not be a valid path - nothing downstream
    // (pathContainsVolumeName expects the "Label (C:)rest" form) understands it.
    return path;
#else
    wstring ws_path = path.ToStdWstring();
    fs::path fs_path;
    std::string pathStr = wstring_to_string(ws_path);
	try {
        fs_path = fs::path(pathStr);
	}
	catch (std::exception& e) {
		log_error(L"Fehler beim Ermitteln des Volumennamens: " + string_to_wstring(e.what()));
		return path;
	}
    if (fs_path.empty() || !fs_path.has_root_name())
		return path;
    wstring rootName = fs_path.root_name().wstring();
    wstring relativePath = ws_path.substr(rootName.size());
    TCHAR volumeName[MAX_PATH + 1] = { 0 };
    TCHAR fileSystemName[MAX_PATH + 1] = { 0 };
    DWORD serialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;
    if (GetVolumeInformation(
        rootName.c_str(),
        volumeName,
        ARRAYSIZE(volumeName),
        &serialNumber,
        &maxComponentLength,
        &fileSystemFlags,
        fileSystemName,
        ARRAYSIZE(fileSystemName)
    )) {
        std::wstringstream os;
        os << volumeName << L" (" << rootName << L")" << relativePath;
        return os.str();
    }
    else {
        return path;
    }
#endif
}

wxString chooseFile(SyncItemDialog* syncItemDialog)
{
    wxFileDialog dialog
    (
        syncItemDialog,
        L"Wähle Datei",
        wxEmptyString,
        wxEmptyString,
        wxString::Format
        (
            "Alle Dateien (%s)|%s",
            wxFileSelectorDefaultWildcardStr,
            wxFileSelectorDefaultWildcardStr
        )
    );
    dialog.CentreOnParent();
    dialog.SetDirectory(wxGetHomeDir());
    if (dialog.ShowModal() == wxID_OK)
    {
        return withVolumenName(dialog.GetPath());
    }
    return "";
}

wxString chooseDir(SyncItemDialog* syncItemDialog)
{
    wxString dirHome;
    wxGetHomeDir(&dirHome);
    wxDirDialog dialog(syncItemDialog, L"Wähle Verzeichnis", dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK)
    {
        return withVolumenName(dialog.GetPath());
    }
    return "";
}

SyncItemType SyncItemDialog::actSyncItemType(wxChoice* typeChoice)
{
    auto selection = typeChoice->GetSelection();
    wxString stype;
    if (selection != wxNOT_FOUND) {
        stype = typeChoice->GetString(selection);
        return str_to_sync_item_type(stype.ToStdWstring());
    }
    else
        return SyncItemType::Undefined;
}

void SyncItemDialog::onPickSourceDirText(wxCommandEvent& WXUNUSED(event))
{
    SyncItemType syncType = actSyncItemType(typeChoice_);
    wxString value;
    if (syncType == SyncItemType::File)
        value = chooseFile(this);
    else
        value = chooseDir(this);
    if (!value.empty())
        sourceDirBox_->SetValue(value);
}

void SyncItemDialog::onPickTargetDirText(wxCommandEvent& WXUNUSED(event))
{
    wxString value = chooseDir(this);
    if (!value.empty())
        targetDirBox_->SetValue(value);
}

bool SyncItemDialog::Validate()
{
    auto name = nameBox_->GetValue();
    if (name.IsEmpty()) {
        wxMessageBox("Der Name darf nicht leer sein.", "Fehler");
        return false;
    }
    // name_ is the original name (set in setData(), still unchanged here since
    // TransferDataFromWindow() only runs after Validate() succeeds). Comparing
    // against it - not just checking name_.empty() - also catches renaming an
    // existing item onto another item's name, not just brand-new items.
    if (name != name_ && Config::instance().find_by_name(name.ToStdWstring()) != std::nullopt) {
        wxMessageBox("Synchronisation " + name + " existiert schon.", "Fehler");
        return false;
    }
    return true;
}

void SyncItemDialog::onSshSource(wxCommandEvent& event)
{
    handleSshButton(sourceDirBox_);
}

void SyncItemDialog::onSshTarget(wxCommandEvent& event)
{
    handleSshButton(targetDirBox_);
}

void SyncItemDialog::handleSshButton(wxTextCtrl* targetBox)
{
    SshAccount account;
    wxString currentValue = targetBox->GetValue();
    
    // Prüfe ob der aktuelle Wert einen SSH-Pfad enthält (Format: sshname:/path)
    int colonPos = currentValue.Find(':');
    if (colonPos != wxNOT_FOUND && colonPos > 0) {
        wxString sshName = currentValue.Left(colonPos);
        // Prüfe ob dieser Name ein bekannter SSH-Account ist
        auto found = Config::instance().find_ssh_account_by_name(std::string(sshName.ToUTF8()));
        if (found.has_value())
            account = found.value();
    }
    
    // Wenn kein Account gefunden wurde, zeige den Auswahldialog
    if (account.name().empty()) {
        SshAccountSelectDialog selectDlg(this);
        if (selectDlg.ShowModal() == wxID_OK) {
            auto selectedAccount = selectDlg.getSelectedAccount();
            if (selectedAccount.has_value()) {
                account = selectedAccount.value();
            } else {
                return;  // Abgebrochen oder kein Account ausgewählt
            }
        } else {
            return;  // Abgebrochen
        }
    }
    
    // Extrahiere den aktuellen Remote-Pfad falls vorhanden
    wxString initialPath = "/";
    if (colonPos != wxNOT_FOUND) {
        initialPath = currentValue.Mid(colonPos + 1);
        if (initialPath.empty()) {
            initialPath = "/";
        }
    }
    
    // Zeige den SSH-Verzeichnisdialog
    SshDirDialog dirDlg(this, L"SSH-Verzeichnis wählen", account, std::string(initialPath.ToUTF8()));
    if (dirDlg.ShowModal() == wxID_OK) {
        wxString selectedPath = dirDlg.getPath();
        // Kombiniere SSH-Account-Name mit Pfad
        wxString fullPath = wxString::FromUTF8(account.name()) + ":" + selectedPath;
        targetBox->SetValue(fullPath);
    }
}
