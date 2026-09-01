#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <string>
#include <optional>
#include <vector>
#include "SshAccount.h"
#include "SshDirInspector.h"

class SshDirDialog : public wxDialog {
public:
    SshDirDialog(wxWindow* parent, 
        const wxString& title,
        const SshAccount& account,
        const wxString& defaultPath = "/",
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~SshDirDialog();

    wxString getPath() const { return selectedPath_; }
    const SshAccount& getAccount() const { return account_; }

private:
    void populateList(const wxString& path);
    void renderList();
    void onListDClick(wxMouseEvent& event);
    void onListSelect(wxListEvent& event);
    void onPathEnter(wxCommandEvent& event);
    void onHomeButton(wxCommandEvent& event);
    void onSortByName(wxCommandEvent& event);

    SshAccount account_;
    wxString currentPath_;
    wxString selectedPath_;
    std::vector<RemoteDirEntry> entries_;
    bool sortAscending_ = true;

    wxTextCtrl* pathBox_ = nullptr;
    wxListCtrl* listCtrl_ = nullptr;
    wxButton* homeButton_ = nullptr;
    wxButton* sortButton_ = nullptr;
    wxStaticText* statusText_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_DIR_PATH_BOX = wxID_HIGHEST + 300,
    ID_SSH_DIR_LIST,
    ID_SSH_DIR_HOME_BUTTON,
    ID_SSH_DIR_SORT_BUTTON
};
