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
namespace AFS_Core { class SyncControler; }

using namespace AFS_Core;

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
    wxStaticText* current_file_ = nullptr;
    wxTimer timer_;
    uint32_t range_ = 0;
    DECLARE_EVENT_TABLE()
};
