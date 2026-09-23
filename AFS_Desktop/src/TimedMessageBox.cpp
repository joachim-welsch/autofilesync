/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
