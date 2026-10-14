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
#include <algorithm>

using namespace AFS_Core;

namespace {
constexpr uint32_t FILE_STEPS = 100;  // [ProgressDialog.md#file-progress]
}

BEGIN_EVENT_TABLE(ProgressDialog, wxDialog)
    EVT_UPDATE_UI(wxID_CANCEL, ProgressDialog::on_update_cancel_button)
    EVT_BUTTON(wxID_CANCEL, ProgressDialog::on_cancel)
END_EVENT_TABLE()

ProgressDialog::ProgressDialog(wxWindow* parent, wxWindowID id,
    const wxString& title,
    SyncControler& sc,
    uint32_t n_files,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
    : wxDialog(parent, id, title, pos, size, style, name)
    , sync_controler_(sc)
    , range_(n_files * FILE_STEPS)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    gauge_ = new wxGauge(this, wxID_ANY, range_, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL, wxDefaultValidator, wxASCII_STR(wxGaugeNameStr));
    gauge_->SetValue(0);
    main_sizer->Add(gauge_, 1, wxEXPAND | wxALL, 10);
    current_file_ = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);  // long paths must not widen the dialog
    main_sizer->Add(current_file_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    wxSizer* button_sizer = CreateButtonSizer(wxCANCEL);
    main_sizer->Add(button_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    SetSizer(main_sizer);
    SetMinSize(FromDIP(wxSize(500, 100)));
    Fit();
    timer_.Bind(wxEVT_TIMER, &ProgressDialog::on_timer_update, this);
    timer_.Start(100);
}

void ProgressDialog::OnCloseWindow(wxCloseEvent&)
{
    closed_ = true;
    wxDialog::Close();
}

ProgressDialog::~ProgressDialog()
{
    timer_.Stop(); // must not fire on_timer_update() on an object mid-destruction
}

void ProgressDialog::on_update_cancel_button(wxUpdateUIEvent& event)
{
    event.Enable(!can_be_closed_);
}

void ProgressDialog::on_cancel(wxCommandEvent& event)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);  // [ProgressDialog.md#lock-released-before-wait]
        can_be_closed_ = true;
        timer_.Stop();
        gauge_->SetValue(range_);
    }
    sync_controler_.request_stop();  // [ProgressDialog.md#cancel-waits-here]
    wxBusyCursor busy_cursor;
    while (!sync_controler_.sync_finished())
    {
        wxSafeYield(this);
        wxMilliSleep(50);
    }
    event.Skip();
}

void ProgressDialog::on_timer_update(wxTimerEvent&)
{
    std::lock_guard<std::mutex> lock(mutex_);  // [ProgressDialog.md#no-yield-under-lock]
    const SyncMessage sm = sync_controler_.get_message();
    if (!sm.msg().empty()) {
        files_done_ = static_cast<uint32_t>(sm.filenumber());
        file_size_ = sm.filesize();
    }
    if (!sm.file().empty())
        current_file_->SetLabelText(sm.file());
    gauge_->SetValue(static_cast<int>(files_done_ * FILE_STEPS + file_steps_done()));  // [ProgressDialog.md#file-progress]
}

uint32_t ProgressDialog::file_steps_done() const
{
    if (file_size_ == 0)
        return 0;
    return static_cast<uint32_t>(std::min<uint64_t>(FILE_STEPS, FILE_STEPS * sync_controler_.copied_bytes() / file_size_));
}

void ProgressDialog::can_be_closed(bool yes_no)
{
    if (!wxThread::IsMain()) {  // [ProgressDialog.md#gui-thread-only]
        CallAfter(&ProgressDialog::can_be_closed, yes_no);
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    can_be_closed_ = yes_no;
    if (can_be_closed_) {
        if (timer_.IsRunning())
            timer_.Stop();
        gauge_->SetValue(range_);
    }
}
