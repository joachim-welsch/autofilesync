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
#include <wx/wx.h>
#include <wx/treectrl.h>
#endif

#include <string>
#include "SyncItem.h"

class Tree;
class Canvas;

using namespace AFS_Core;

enum class TreeCtrlIcon
{
    File,
    FileSelected,
    Folder,
    FolderSelected,
    FolderOpened
};

class TreeCtrl: public wxTreeCtrl {
    enum
    {
        TreeCtrlIcon_File,
        TreeCtrlIcon_FileSelected,
        TreeCtrlIcon_Folder,
        TreeCtrlIcon_FolderSelected,
        TreeCtrlIcon_FolderOpened
    };

    bool alternateImages_ = false; 
    bool alternateStates_ = false;
    bool reverseSort_ = false;
    int imageSize_ = 16;
    wxTreeItemId rootId_;
    bool mouseClicked_ = false;
public:
    TreeCtrl() = default;
    TreeCtrl(Canvas* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
    virtual ~TreeCtrl() = default;
    void update();
    wxTreeItemId rootId() const;
    wxTreeItemId addBranch(const std::wstring& l, Tree& tree, SyncItemType);
    bool mouseClicked();
    void ExpandNode(wxTreeItemId node);
private:

    void CreateStateImageList(bool del);
    void CreateImageList(int size = 16);
    void AddRootItemToTree();

    void OnMouseLeftDown(wxMouseEvent& evt);
    Canvas* parent_;
    wxDECLARE_EVENT_TABLE();
};

