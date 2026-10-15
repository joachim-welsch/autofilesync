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

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>
#include "SyncPlan.h"

using namespace AFS_Core;

class PreviewDialog: public wxDialog {  // [PreviewDialog.md#PreviewDialog]
    wxListCtrl* list_ = nullptr;
    void fill(const SyncPlan& plan);
    void add_step(const PlannedStep& step);
public:
    PreviewDialog(wxWindow* parent, const SyncPlan& plan, const wxString& notes);  // [PreviewDialog.md#notes]
    static wxString action_label(PlannedAction action);
    static wxString summary(const SyncPlan& plan);  // [PreviewDialog.md#summary]
    wxListCtrl& list() { return *list_; }
};

enum {
    ID_PREVIEW_LIST = wxID_HIGHEST + 400
};
