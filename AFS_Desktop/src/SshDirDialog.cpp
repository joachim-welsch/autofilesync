#include "SshDirDialog.h"
#include "SshDirInspector.h"
#include <wx/busyinfo.h>
#include <algorithm>

using namespace AFS_Core;

BEGIN_EVENT_TABLE(SshDirDialog, wxDialog)
    EVT_BUTTON(ID_SSH_DIR_HOME_BUTTON, SshDirDialog::onHomeButton)
    EVT_BUTTON(ID_SSH_DIR_SORT_BUTTON, SshDirDialog::onSortByName)
    EVT_TEXT_ENTER(ID_SSH_DIR_PATH_BOX, SshDirDialog::onPathEnter)
    EVT_LIST_ITEM_SELECTED(ID_SSH_DIR_LIST, SshDirDialog::onListSelect)
END_EVENT_TABLE()

SshDirDialog::SshDirDialog(wxWindow* parent, 
    const wxString& title,
    const SshAccount& account,
    const wxString& defaultPath,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxDialog(parent, wxID_ANY, title, pos, size, style)
    , account_(account)
    , currentPath_(defaultPath)
    , selectedPath_(defaultPath)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* connInfo = new wxStaticText(this, wxID_STATIC, 
        wxString::Format(_("Verbindung: %s@%s"), wxString::FromUTF8(account_.user()),
            wxString::FromUTF8(account_.host())));
    mainSizer->Add(connInfo, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* pathSizer = new wxBoxSizer(wxHORIZONTAL);
    
    homeButton_ = new wxButton(this, ID_SSH_DIR_HOME_BUTTON, _("Home"),
        wxDefaultPosition, wxSize(50, -1));
    pathSizer->Add(homeButton_, 0, wxRIGHT, 2);

    sortButton_ = new wxButton(this, ID_SSH_DIR_SORT_BUTTON, _("A-Z"),
        wxDefaultPosition, wxSize(35, -1));
    pathSizer->Add(sortButton_, 0, wxRIGHT, 5);

    pathBox_ = new wxTextCtrl(this, ID_SSH_DIR_PATH_BOX, currentPath_,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    pathSizer->Add(pathBox_, 1, wxEXPAND);
    
    mainSizer->Add(pathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    listCtrl_ = new wxListCtrl(this, ID_SSH_DIR_LIST, wxDefaultPosition,
        wxSize(500, 300), wxLC_REPORT | wxLC_SINGLE_SEL);
    
    listCtrl_->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 250);
    listCtrl_->InsertColumn(1, _(L"Größe"), wxLIST_FORMAT_RIGHT, 80);
    listCtrl_->InsertColumn(2, _(L"Geändert"), wxLIST_FORMAT_LEFT, 150);
    
    mainSizer->Add(listCtrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    
    listCtrl_->Bind(wxEVT_LEFT_DCLICK, &SshDirDialog::onListDClick, this);

    statusText_ = new wxStaticText(this, wxID_STATIC, _(""));
    mainSizer->Add(statusText_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(550, 400));
    Fit();
    Centre();

    populateList(currentPath_);
}

SshDirDialog::~SshDirDialog()
{}

void SshDirDialog::populateList(const wxString& path)
{
    listCtrl_->DeleteAllItems();
    statusText_->SetLabel(_("Lade Verzeichnis..."));
    Update();

    try {
        wxBusyCursor wait;
        SshDirInspector inspector(account_);
        RemoteDirInfo info = inspector.inspect_dir(std::string(path.ToUTF8()));

        if (!info.exists) {
            statusText_->SetLabel(wxString::Format(_("Fehler: %s"), info.error_message));
            return;
        }

        if (!info.is_directory) {
            statusText_->SetLabel(_("Pfad ist kein Verzeichnis"));
            return;
        }

        currentPath_ = path;
        pathBox_->SetValue(currentPath_);

        entries_ = std::move(info.entries);
        renderList();

        statusText_->SetLabel(wxString::Format(_(L"%zu Einträge"), entries_.size()));

    } catch (const std::exception& ex) {
        statusText_->SetLabel(wxString::Format(_("Fehler: %s"), ex.what()));
    }
}

void SshDirDialog::renderList()
{
    listCtrl_->DeleteAllItems();

    long index = 0;
    for (const auto& entry : entries_) {
        wxString displayName = wxString::FromUTF8(entry.name);  // remote file names arrive as UTF-8 from the SFTP server
        if (entry.is_directory) {
            displayName = "[" + displayName + "]";
        }

        long item = listCtrl_->InsertItem(index, displayName);

        if (entry.is_regular_file) {
            wxString sizeStr;
            if (entry.size_bytes < 1024) {
                sizeStr = wxString::Format("%llu B", entry.size_bytes);
            } else if (entry.size_bytes < 1024 * 1024) {
                sizeStr = wxString::Format("%.1f KB", entry.size_bytes / 1024.0);
            } else {
                sizeStr = wxString::Format("%.1f MB", entry.size_bytes / (1024.0 * 1024.0));
            }
            listCtrl_->SetItem(item, 1, sizeStr);
        } else if (entry.is_directory) {
            listCtrl_->SetItem(item, 1, _("<DIR>"));
        }

        listCtrl_->SetItem(item, 2, entry.last_modified_iso);

        listCtrl_->SetItemData(item, entry.is_directory ? 1 : 0);

        index++;
    }
}

void SshDirDialog::onListDClick(wxMouseEvent&)
{
    long sel = listCtrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1)
        return;
    
    if (listCtrl_->GetItemData(sel) == 1) {
        wxString itemText = listCtrl_->GetItemText(sel);
        if (itemText.StartsWith("[") && itemText.EndsWith("]")) {
            itemText = itemText.Mid(1, itemText.Length() - 2);
        }
        
        wxString newPath = currentPath_;
        if (!newPath.EndsWith("/")) {
            newPath += "/";
        }
        newPath += itemText;
        
        populateList(newPath);
    }
}

void SshDirDialog::onListSelect(wxListEvent& event)
{
    long sel = event.GetIndex();
    if (sel == -1)
        return;
    
    wxString itemText = listCtrl_->GetItemText(sel);
    if (itemText.StartsWith("[") && itemText.EndsWith("]")) {
        itemText = itemText.Mid(1, itemText.Length() - 2);
    }
    
    selectedPath_ = currentPath_;
    if (!selectedPath_.EndsWith("/")) {
        selectedPath_ += "/";
    }
    selectedPath_ += itemText;
}

void SshDirDialog::onPathEnter(wxCommandEvent&)
{
    wxString newPath = pathBox_->GetValue();
    if (!newPath.IsEmpty()) {
        populateList(newPath);
    }
}

void SshDirDialog::onHomeButton(wxCommandEvent&)
{
    try {
        wxBusyCursor wait;
        SshDirInspector inspector(account_);
        populateList(wxString::FromUTF8(inspector.get_home_dir()));
    } catch (const std::exception& ex) {
        statusText_->SetLabel(wxString::Format(_("Fehler: %s"), ex.what()));
    }
}

void SshDirDialog::onSortByName(wxCommandEvent&)
{
    bool ascending = sortAscending_;
    std::sort(entries_.begin(), entries_.end(), [ascending](const RemoteDirEntry& a, const RemoteDirEntry& b) {
        int cmp = wxString::FromUTF8(a.name).CmpNoCase(wxString::FromUTF8(b.name));
        return ascending ? cmp < 0 : cmp > 0;
    });
    sortAscending_ = !sortAscending_;
    renderList();
}
