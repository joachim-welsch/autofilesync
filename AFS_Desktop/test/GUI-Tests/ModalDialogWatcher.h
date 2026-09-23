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
#pragma once

#include <wx/dialog.h>
#include <wx/modalhook.h>
#include <deque>
#include <functional>
#include <vector>

namespace guitest {

using DialogAction = std::function<void(wxDialog&)>;

class ModalDialogWatcher: public wxModalDialogHook {  // [ModalDialogWatcher.md#ModalDialogWatcher]
public:
    static ModalDialogWatcher& instance();
    void expect(const wxString& title, DialogAction action);
    void choosePath(const wxString& title, const wxString& path);  // [ModalDialogWatcher.md#choosePath]
    void answer(const wxString& title, int buttonId);  // wxID_OK, wxID_YES, wxID_NO ... - the dialog's return code, not the wxYES/wxNO style flags [ModalDialogWatcher.md#answer]
    bool hasPendingExpectations() const { return !expected_.empty(); }
    const std::vector<wxString>& unexpectedDialogs() const { return unexpected_; }
    const std::vector<wxString>& answeredMessages() const { return answered_; }
    void reset();
protected:
    int Enter(wxDialog* dialog) override;
private:
    struct Expectation { wxString title; DialogAction action; int answer = wxID_NONE; wxString path; };
    std::deque<Expectation> expected_;
    std::vector<wxString> unexpected_;
    std::vector<wxString> answered_;
};

}
