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
#include "TaskBarIcon.h"
#include "MainWindow.h"
#include "Translation.h"
#include "../icons/smile.xpm"

enum
{
    PU_RESTORE = 10001,
    PU_EXIT,
    PU_CHECKMARK,
    PU_SUBMAIN
};

wxBEGIN_EVENT_TABLE(TaskBarIcon, wxTaskBarIcon)
EVT_MENU(PU_RESTORE, TaskBarIcon::on_menu_restore)
EVT_MENU(PU_EXIT, TaskBarIcon::on_menu_exit)
EVT_MENU(PU_CHECKMARK, TaskBarIcon::on_menu_checkmark)
EVT_UPDATE_UI(PU_CHECKMARK, TaskBarIcon::on_menu_ui_checkmark)
EVT_TASKBAR_LEFT_DCLICK(TaskBarIcon::on_left_button_double_click)
wxEND_EVENT_TABLE()

void TaskBarIcon::on_menu_restore(wxCommandEvent&)
{
    if (main_window_ != nullptr)
        main_window_->Show(true);
}

void TaskBarIcon::on_menu_exit(wxCommandEvent&)
{
    if (main_window_ != nullptr)
        main_window_->Close(true);
}

static bool check = true;

void TaskBarIcon::on_menu_checkmark(wxCommandEvent&)
{
    check = !check;
}

void TaskBarIcon::on_menu_ui_checkmark(wxUpdateUIEvent& event)
{
    event.Check(check);
}

void TaskBarIcon::on_menu_set_new_icon(wxCommandEvent&)
{
    wxIcon icon(smile_xpm);

    if (!SetIcon(icon, "wxTaskBarIcon Sample - a different icon"))
        wxMessageBox("Could not set new icon.");
}

void TaskBarIcon::on_menu_sub(wxCommandEvent&)
{
    wxMessageBox("You clicked on a submenu!");
}

wxMenu* TaskBarIcon::CreatePopupMenu()
{
    wxMenu* menu = new wxMenu;
    menu->Append(PU_RESTORE, AFS_Core::translation::translate("taskbar_restore"));
#ifdef __WXOSX__  // OSX has a built-in quit menu for the dock menu, but not for the status item
    if (OSXIsStatusItem())
#endif
    {
        menu->AppendSeparator();
        menu->Append(PU_EXIT, AFS_Core::translation::translate("taskbar_exit"));
    }
    return menu;
}

void TaskBarIcon::on_left_button_double_click(wxTaskBarIconEvent&)
{
    if (main_window_ != nullptr)
        main_window_->Show(true);
}
