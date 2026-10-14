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
#include <wx/wx.h>
#include <wx/treebase.h>
#endif

#include <string>

class GridCtrl;

class GridWindow: public wxScrolledWindow {
public:
    GridWindow(wxWindow* parent);
    GridWindow() {}
    std::wstring get_selected() const;
    void update();

    void on_context_menu(wxContextMenuEvent& event);
    void on_size(wxSizeEvent& event);
private:
    void show_context_menu(const wxPoint& pos);

    GridCtrl* grid_ctrl_ = nullptr;
    wxDECLARE_NO_COPY_CLASS(GridWindow);
    wxDECLARE_EVENT_TABLE();
};


