#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

class SettingsDialog: public wxDialog {
public:
    SettingsDialog(
        wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SettingsDialog();
private:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    void onPickRecycleBinPath(wxCommandEvent&);
    void onDeleteModeChanged(wxCommandEvent&);
    void enableRecycleBinPath();

    wxRadioBox* deleteModeRB_ = nullptr;
    wxTextCtrl* recycleBinPathBox_ = nullptr;
    wxButton*   recycleBinPathBtn_ = nullptr;
    wxCheckBox* startMinimizedCB_ = nullptr;
    wxCheckBox* minimizeInsteadOfCloseCB_ = nullptr;
    wxCheckBox* autoStartCB_ = nullptr;
    wxCheckBox* startSyncAtStartCB_ = nullptr;
    wxCheckBox* messageBoxErrorCB_ = nullptr;
    wxCheckBox* messageBoxWarningCB_ = nullptr;
    wxCheckBox* messageBoxInfoCB_ = nullptr;
    DECLARE_EVENT_TABLE()
};

enum {
    ID_DELETE_MODE_RADIO = wxID_HIGHEST + 1,
    ID_RECYCLEBIN_PATH_BOX,
    ID_RECYCLEBIN_PATH_BUTTON
};

