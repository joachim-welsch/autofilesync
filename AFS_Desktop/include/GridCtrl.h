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
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#endif
#include "wx/grid.h"

class GridCtrl: public wxGrid {
public:
    GridCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size);

    void addDataItems();
    std::wstring getSelected() const;
    void update();
    void adjustColumnWidths(int windowWidth);
private:
    void OnCellValueChanged(wxGridEvent&);
    void OnModifyEntry(wxGridEvent&);
    wxDECLARE_EVENT_TABLE();
};

