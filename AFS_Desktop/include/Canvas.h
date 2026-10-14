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
#include <vector>
#include <map>
#include "Tree.h"
#include "Global.h"

class TreeCtrl;

class Canvas: public wxScrolledWindow {
public:
    Canvas(wxWindow* parent, const wxString& heading);
    virtual ~Canvas(){}
    virtual void on_select_root() = 0;
    virtual void update() = 0;
    virtual bool source() const = 0;
    virtual void add_data_items() = 0;
    std::wstring selected_path() const;
    bool mouse_clicked() const;
    void other(Canvas* o) { other_ = o; }
    Canvas* other() const { return other_; }
    void deselect();
    bool select_path(const std::wstring& path);  // [Canvas.md#select_path]
    void on_item_left_clicked(wxTreeItemId item);  // [Canvas.md#on_item_left_clicked]
protected:
    wxStaticText* heading_ = nullptr;
    TreeCtrl* tree_ctrl_ = nullptr;
    Tree tree_;
    Canvas* other_ = nullptr;
private:
    void show_context_menu(const wxPoint& pos);
    void on_right_up(wxMouseEvent& event);
    void on_context_menu(wxContextMenuEvent& event);
    void on_size(wxSizeEvent& event);  // [Canvas.md#on_size]

    wxDECLARE_NO_COPY_CLASS(Canvas);
    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_SCROLLED_WIN = wxID_HIGHEST + 1,
    ID_SRC_TREE_CTRL
};
