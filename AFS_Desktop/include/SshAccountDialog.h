#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include <optional>

class SshAccount;

// Shared with SshAccountsDialog's inline label edit, which otherwise bypasses
// this dialog's validation entirely. Returns the German error message if name
// is invalid, nullopt if it's fine.
std::optional<wxString> validateSshAccountName(const wxString& name);

class SshAccountDialog : public wxDialog {
public:
    SshAccountDialog(SshAccount* sshAccount,
        wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SshAccountDialog();

    wxString name() const { return name_; }
    wxString user() const { return user_; }
    wxString host() const { return host_; }
    wxString password() const { return password_; }

    void saveData(SshAccount& account);

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    void onUpdateOkButton(wxUpdateUIEvent& event);
    void setData(const SshAccount& account);

    wxString name_;
    wxString user_;
    wxString host_;
    wxString password_;

    wxTextCtrl* nameBox_ = nullptr;
    wxTextCtrl* userBox_ = nullptr;
    wxTextCtrl* hostBox_ = nullptr;
    wxTextCtrl* passwordBox_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_SSH_NAME_BOX = wxID_HIGHEST + 100,
    ID_SSH_USER_BOX,
    ID_SSH_HOST_BOX,
    ID_SSH_PASSWORD_BOX
};
