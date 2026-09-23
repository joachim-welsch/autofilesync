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
