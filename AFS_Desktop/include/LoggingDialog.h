#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif
#include "wx/spinctrl.h"
#include "wx/notebook.h"

class LoggingDialog: public wxDialog {
public:
    LoggingDialog(wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~LoggingDialog();

private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    wxPanel* createLoggingPage(wxNotebook* notebook);
    wxPanel* createEmailPage(wxNotebook* notebook);
    void addField(wxPanel* page, wxSizer* pageSizer, const wxString& label,
        wxWindowID id, wxTextCtrl*& box, long style = 0);
    void onNotifyActiveChanged(wxCommandEvent&);
    void onPickBackupProofPath(wxCommandEvent&);
    void updateNotifyEnabling();

    wxTextCtrl* logPathBox_ = nullptr;
    wxListBox*  logLevelListbox_ = nullptr;
    wxSpinCtrl* retentionDaysCtrl_ = nullptr;
    wxTextCtrl* editPathBox_ = nullptr;
    wxTextCtrl* backupProofPathBox_ = nullptr;
    wxCheckBox* emailActiveCheck_ = nullptr;
    wxTextCtrl* emailBox_ = nullptr;
    wxTextCtrl* smtpServerBox_ = nullptr;  // the server the notification is sent through
    wxSpinCtrl* smtpPortCtrl_ = nullptr;
    wxTextCtrl* smtpUserBox_ = nullptr;
    wxTextCtrl* smtpPasswordBox_ = nullptr;
    wxTextCtrl* smtpFromBox_ = nullptr;

DECLARE_EVENT_TABLE()
};

enum {
    ID_LOG_PATH_BOX = wxID_HIGHEST + 1,
    ID_LOG_LEVEL_LISTBOX,
    ID_RETENTIONDAYS_SPIN,
    ID_EDIT_PATH_BOX,
    ID_BACKUP_PROOF_PATH_BOX,
    ID_BTN_PICK_BACKUP_PROOF_PATH,
    ID_EMAIL_ACTIVE_CHECK,
    ID_EMAIL_BOX,
    ID_SMTP_SERVER_BOX,
    ID_SMTP_PORT_SPIN,
    ID_SMTP_USER_BOX,
    ID_SMTP_PASSWORD_BOX,
    ID_SMTP_FROM_BOX
};
