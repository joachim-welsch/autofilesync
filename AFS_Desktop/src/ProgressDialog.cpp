#include "ProgressDialog.h"
#include "Synchronize.h"
#include "MainWindow.h"
#include "SyncControler.h"

using namespace AFS_Core;

BEGIN_EVENT_TABLE(ProgressDialog, wxDialog)
    EVT_UPDATE_UI(wxID_CANCEL, ProgressDialog::onUpdateCancelButton)
    EVT_BUTTON(wxID_CANCEL, ProgressDialog::OnCancel)
END_EVENT_TABLE()

ProgressDialog::ProgressDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    SyncControler& sc,
    uint32_t range,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
	, syncControler_(sc)
    , range_(range)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    gauge_ = new wxGauge(this, wxID_ANY, range_, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL, wxDefaultValidator, wxASCII_STR(wxGaugeNameStr));
    gauge_->SetValue(0);
    mainSizer->Add(gauge_, 1, wxEXPAND | wxALL, 10);
    wxSizer* buttonSizer = CreateButtonSizer(wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    SetSizer(mainSizer);
    SetMinSize(wxSize(500, 100));
    Fit();
    timer_.Bind(wxEVT_TIMER, &ProgressDialog::onTimerUpdate, this);
    timer_.Start(100);
}

void ProgressDialog::OnCloseWindow(wxCloseEvent&)
{
    closed_ = true;
    wxDialog::Close();
}

ProgressDialog::~ProgressDialog()
{
    timer_.Stop(); // must not fire onTimerUpdate() on an object mid-destruction
}

void ProgressDialog::onUpdateCancelButton(wxUpdateUIEvent& event)
{
    event.Enable(!canBeClosed_);
}

void ProgressDialog::OnCancel(wxCommandEvent& event)
{
    {
        // Released before the wait loop below: wxSafeYield() there pumps the
        // event queue on this same (GUI) thread, and any CallAfter()-queued
        // call into e.g. setProgress()/canBeClosed() that also locks mutex_
        // would deadlock against a still-held, non-recursive lock.
        std::lock_guard<std::mutex> lock(mutex_);
        canBeClosed_ = true;
        timer_.Stop();
        gauge_->SetValue(range_);
        waiting_ = true;
    }
    // requestStop() only signals; the actual wait happens here, not inside
    // SyncControler::stopSync()'s std::this_thread::sleep_for() loop, which
    // blocks the GUI thread outright - clicking Cancel used to freeze the
    // whole window for as long as the current copy step took to notice the
    // stop request.
    syncControler_.request_stop();
    wxBusyCursor busyCursor;
    while (!syncControler_.sync_finished())
    {
        wxSafeYield(this);
        wxMilliSleep(50);
    }
	event.Skip();
}

void ProgressDialog::onTimerUpdate(wxTimerEvent&)
{
    std::lock_guard<std::mutex> lock(mutex_);
    wxYield();
    SyncMessage sm = syncControler_.get_message();
    if (!sm.msg().empty())
        gauge_->SetValue(sm.filenumber());
}

void ProgressDialog::canBeClosed(bool yesNo)
{
    // gauge_ is a wx widget and may only be touched on the GUI thread; callers
    // (e.g. the sync worker thread) get marshalled there via CallAfter().
    if (!wxThread::IsMain()) {
        CallAfter(&ProgressDialog::canBeClosed, yesNo);
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    canBeClosed_ = yesNo;
    if (canBeClosed_) {
		if (timer_.IsRunning())
            timer_.Stop();
        gauge_->SetValue(range_);
    }
}

void ProgressDialog::setRange(uint32_t range)
{
    if (!wxThread::IsMain()) {
        CallAfter(&ProgressDialog::setRange, range);
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    range_ = range;
    timer_.Stop();
    gauge_->SetRange(range);
	gauge_->SetValue(0);
}

void ProgressDialog::setProgress(uint32_t value)
{
    if (!wxThread::IsMain()) {
        CallAfter(&ProgressDialog::setProgress, value);
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
	if (waiting_) {
		return;
	}
    if (value <= range_) {
        gauge_->SetValue(value);
    }
}