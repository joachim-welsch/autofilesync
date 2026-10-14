/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
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
    current_file_ = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);  // long paths must not widen the dialog
    mainSizer->Add(current_file_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    wxSizer* buttonSizer = CreateButtonSizer(wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    SetSizer(mainSizer);
    SetMinSize(FromDIP(wxSize(500, 100)));
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
        std::lock_guard<std::mutex> lock(mutex_);  // [ProgressDialog.md#lock-released-before-wait]
        canBeClosed_ = true;
        timer_.Stop();
        gauge_->SetValue(range_);
        waiting_ = true;
    }
    syncControler_.request_stop();  // [ProgressDialog.md#cancel-waits-here]
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
    std::lock_guard<std::mutex> lock(mutex_);  // [ProgressDialog.md#no-yield-under-lock]
    SyncMessage sm = syncControler_.get_message();
    if (!sm.msg().empty())
        gauge_->SetValue(sm.filenumber());
    if (!sm.file().empty())
        current_file_->SetLabelText(sm.file());
}

void ProgressDialog::canBeClosed(bool yesNo)
{
    if (!wxThread::IsMain()) {  // [ProgressDialog.md#gui-thread-only]
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