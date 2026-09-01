#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>

class SshAccountsDialog : public wxDialog {
public:
    SshAccountsDialog(wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SshAccountsDialog();

private:
    void populateList();
    void updateButtons();
    
    void onAdd(wxCommandEvent& event);
    void onEdit(wxCommandEvent& event);
    void onDelete(wxCommandEvent& event);
    void onListDClick(wxMouseEvent& event);
    void onListSelect(wxListEvent& event);
    void onListDeselect(wxListEvent& event);
    void onEndLabelEdit(wxListEvent& event);
    
    void editSelectedItem();
    long getSelectedIndex() const;

    wxListCtrl* listCtrl_ = nullptr;
    wxButton* addButton_ = nullptr;
    wxButton* editButton_ = nullptr;
    wxButton* deleteButton_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSHACCOUNTS_LIST = wxID_HIGHEST + 200,
    ID_SSHACCOUNTS_ADD,
    ID_SSHACCOUNTS_EDIT,
    ID_SSHACCOUNTS_DELETE
};
