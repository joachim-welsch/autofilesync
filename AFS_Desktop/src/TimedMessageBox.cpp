#include "TimedMessageBox.h"

TimedMessageBox::TimedMessageBox(wxWindow* parent, const wxString& message, const wxString& caption, int timeoutMs)
    : wxDialog(parent, wxID_ANY, caption, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* text = new wxStaticText(this, wxID_ANY, message, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL | wxST_NO_AUTORESIZE);
    sizer->Add(text, 1, wxALL | wxEXPAND, 15);
    SetSizer(sizer);
    Fit();

    Centre();

    timer_.Bind(wxEVT_TIMER, &TimedMessageBox::OnTimer, this);
    timer_.Start(timeoutMs, wxTIMER_ONE_SHOT); // Auto-close after timeoutMs milliseconds
}

void TimedMessageBox::OnTimer(wxTimerEvent&)
{
    Close();
}
