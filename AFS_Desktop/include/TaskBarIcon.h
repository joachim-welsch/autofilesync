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
#include "wx/wx.h"
#endif

#include "wx/taskbar.h"

class MainWindow;

class TaskBarIcon: public wxTaskBarIcon {
public:
    TaskBarIcon(MainWindow* mwd) : main_window_(mwd) {}
    void on_left_button_double_click(wxTaskBarIconEvent&);
    void on_menu_restore(wxCommandEvent&);
    void on_menu_exit(wxCommandEvent&);
    void on_menu_set_new_icon(wxCommandEvent&);
    void on_menu_checkmark(wxCommandEvent&);
    void on_menu_ui_checkmark(wxUpdateUIEvent&);
    void on_menu_sub(wxCommandEvent&);
    virtual wxMenu* CreatePopupMenu() wxOVERRIDE;
private:
    MainWindow* main_window_ = nullptr;
    wxDECLARE_EVENT_TABLE();

};

