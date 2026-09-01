#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <utility>
#include <string>
#include <cstdint>
#include <atomic>
#include <mutex>

class MainWindow;
class SyncControler;

class ProgressDialog: public wxDialog {
    bool closed_ = false;
    bool canBeClosed_ = false;
	SyncControler& syncControler_;
	std::atomic_bool	waiting_{ false };
    mutable std::mutex mutex_;
public:
    ProgressDialog(wxWindow* parent,
        wxWindowID id,
        const wxString& title,
		SyncControler&,
        uint32_t range,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~ProgressDialog();

    void OnCloseWindow(wxCloseEvent& event);
    bool closed() const { return closed_;  }
    bool canBeClosed() const { return canBeClosed_; }
    void canBeClosed(bool);
	void setRange(uint32_t range); // Switch to progress of single file
	void setProgress(uint32_t value);
private:
    void OnCancel(wxCommandEvent& event);
    void onTimerUpdate(wxTimerEvent&);
    void onUpdateCancelButton(wxUpdateUIEvent& event);

    wxGauge* gauge_ = nullptr;
    wxTimer timer_;
    uint32_t range_ = 0;
    DECLARE_EVENT_TABLE()
};
