#pragma once
#include <wx/wx.h>
#include <wx/timer.h>

class TimedMessageBox: public wxDialog {
	wxTimer timer_;

	void OnTimer(wxTimerEvent&);
public:
	TimedMessageBox(wxWindow* parent, const wxString& message, const wxString& caption, int timeoutMs);
};

