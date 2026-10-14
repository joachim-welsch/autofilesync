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

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/grid.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <filesystem>

namespace guitest {  // [UserActions.md#UserActions]

wxWindow* find(wxWindow& parent, wxWindowID id);
wxString value_of(wxWindow* control);
bool click_on(wxWindow* control);
bool click_on(wxWindow& control, const wxPoint& client_point);  // [UserActions.md#click_on-point]
bool click_on(wxTreeCtrl& tree, wxTreeItemId item);  // [UserActions.md#click_on-tree-item]
bool click_on(wxListCtrl& list, long item, bool double_click = false);  // [UserActions.md#click_on-list-item]
bool click_on_row_label(wxGrid& grid, int row);  // [UserActions.md#click_on_row_label]
bool change_grid_cell(wxGrid& grid, int row, int col, const wxString& value);  // [UserActions.md#change_grid_cell]
bool press_return(int modifiers = wxMOD_NONE);  // [UserActions.md#press_return]
bool type_into(wxWindow* field, const wxString& text);
bool replace_text(wxWindow* field, const wxString& text);  // [UserActions.md#replace_text]
bool replace_lines(wxWindow* field, const wxString& lines);  // [UserActions.md#replace_lines]
bool select_radio_option(wxWindow* radio_box, int option);
bool select_entry(wxWindow* control, const wxString& entry);  // [UserActions.md#select_entry]
bool set_check_box(wxWindow* check_box, bool checked);  // [UserActions.md#set_check_box]
bool set_spin_value(wxWindow* spin, int value);  // [UserActions.md#set_spin_value]
bool show_notebook_page(wxWindow& dialog, const wxString& page_title);  // [UserActions.md#show_notebook_page]
bool click_when_enabled(wxWindow* control, int timeout_milliseconds = 5000);
void activate_menu_item(wxFrame& frame, int menu_item_id);
bool activate_toolbar_tool(wxFrame& frame, int tool_id);  // [UserActions.md#activate_toolbar_tool]
wxTreeItemId find_tree_item_for_path(wxTreeCtrl& tree, const std::filesystem::path& path);  // [UserActions.md#find_tree_item_for_path]

}
