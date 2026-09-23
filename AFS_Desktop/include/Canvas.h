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
    virtual void onSelectRoot() = 0;
    virtual void update() = 0;
    virtual bool source() const = 0;
    virtual void addDataItems() = 0;
    std::wstring selectedPath() const;
    bool mouseClicked() const;
    void other(Canvas* o) { other_ = o; }
    Canvas* other() const { return other_; }
    void deSelect();
    bool selectPath(const std::wstring& path);  // [Canvas.md#selectPath]
    void onItemLeftClicked(wxTreeItemId item);  // [Canvas.md#onItemLeftClicked]
protected:
    wxStaticText* heading_ = nullptr;
    TreeCtrl* treeCtrl_ = nullptr;
    Tree tree_;
    Canvas* other_ = nullptr;
private:
    void ShowContextMenu(const wxPoint& pos);
    void OnRightUp(wxMouseEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnSize(wxSizeEvent& event);  // [Canvas.md#OnSize]

    wxDECLARE_NO_COPY_CLASS(Canvas);
    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_SCROLLED_WIN = wxID_HIGHEST + 1,
    ID_SRC_TREE_CTRL
};
