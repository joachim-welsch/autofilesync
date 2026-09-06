#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>
#include <optional>
#include "SshAccount.h"

// Dialog zur Auswahl einer SSH-Verbindung aus der Liste

using namespace AFS_Core;

class SshAccountSelectDialog : public wxDialog {
public:
    SshAccountSelectDialog(wxWindow* parent,
        const wxString& title = _(L"SSH-Verbindung auswählen"),
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~SshAccountSelectDialog();

    std::optional<SshAccount> getSelectedAccount() const;

private:
    void populateList();
    void onListDClick(wxMouseEvent& event);
    void onListSelect(wxListEvent& event);
    void updateButtons();

    wxListCtrl* listCtrl_ = nullptr;
    int selectedIndex_ = -1;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_SELECT_LIST = wxID_HIGHEST + 350
};
