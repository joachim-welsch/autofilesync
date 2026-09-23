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
#include "TreeCtrl.h"
#include "wx/artprov.h"


#include "../icons/icon1.xpm"
#include "../icons/icon2.xpm"
#include "../icons/icon3.xpm"
#include "../icons/icon4.xpm"
#include "../icons/icon5.xpm"
#include "../icons/state1.xpm"
#include "../icons/state2.xpm"
#include "../icons/state3.xpm"
#include "../icons/state4.xpm"
#include "../icons/state5.xpm"
#include "../icons/unchecked.xpm"
#include "../icons/checked.xpm"

#include "TreeItemData.h"
#include "Config.h"
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "Tree.h"
#include "Canvas.h"

using namespace AFS_Core;

using std::wstring;
using std::vector;
using std::nullopt;

wxBEGIN_EVENT_TABLE(TreeCtrl, wxTreeCtrl)
    EVT_LEFT_DOWN(TreeCtrl::OnMouseLeftDown)
wxEND_EVENT_TABLE()

TreeCtrl::TreeCtrl(Canvas* parent, const wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style)
    : wxTreeCtrl(parent, id, pos, size, style)
    , parent_(parent)
{
    CreateImageList();
    AddRootItemToTree();
}

void TreeCtrl::OnMouseLeftDown(wxMouseEvent& evt)
{
    wxPoint position = evt.GetPosition();
    int flags = 0;
    parent_->onItemLeftClicked(HitTest(position, flags));
    if (position.x >= 0 && position.y >= 0) {
        mouseClicked_ = true;
    }
    evt.Skip();
}

void TreeCtrl::AddRootItemToTree()
{
    rootId_ = AddRoot("PC", TreeCtrlIcon_Folder, TreeCtrlIcon_Folder, new TreeItemData("PC"));
    if (!HasFlag(wxTR_HIDE_ROOT) /* && image != -1*/)
    {
        SetItemImage(rootId_, TreeCtrlIcon_FolderOpened, wxTreeItemIcon_Expanded);
    }
}


void TreeCtrl::CreateImageList(int size)
{
    if (size == -1)
    {
        SetImageList(NULL);
        return;
    }
    if (size == 0)
        size = imageSize_;
    else
        imageSize_ = size;

    wxImageList* images = new wxImageList(size, size, true);

    wxIcon icons[5];  // should correspond to the TreeCtrlIcon_xxx enum
    icons[TreeCtrlIcon_File] = wxIcon(icon1_xpm);
    icons[TreeCtrlIcon_FileSelected] = wxIcon(icon2_xpm);
    icons[TreeCtrlIcon_Folder] = wxIcon(icon3_xpm);
    icons[TreeCtrlIcon_FolderSelected] = wxIcon(icon4_xpm);
    icons[TreeCtrlIcon_FolderOpened] = wxIcon(icon5_xpm);
    for (size_t i = 0; i < WXSIZEOF(icons); i++)
    {
        int sizeOrig = icons[0].GetWidth();
        if (size == sizeOrig)
        {
            images->Add(icons[i]);
        }
        else
        {
            images->Add(wxBitmap(wxBitmap(icons[i]).ConvertToImage().Rescale(size, size)));
        }
    }
    AssignImageList(images);
}

void TreeCtrl::CreateStateImageList(bool /*del*/)
{
}

wxTreeItemId TreeCtrl::rootId() const
{
    return rootId_;
}

wxTreeItemId TreeCtrl::addBranch(const wstring& l, Tree& tree, SyncItemType syncType)
{
    vector<wstring> strs;
    const bool backslashPath = l.find(L'\\') != wstring::npos;  // [TreeCtrl.md#separator-detected-from-path]
    boost::split(strs, l, boost::is_any_of(backslashPath ? "\\" : "/"));
    strs.erase(std::remove_if(strs.begin(), strs.end(), [](const wstring& s) { return s.empty(); }), strs.end());  // [TreeCtrl.md#empty-segments-removed]
    int image = 0;
    wxTreeItemId id = rootId_;
    const wchar_t separator = backslashPath ? L'\\' : L'/';
    wstring path = l.substr(0, l.find_first_not_of(separator));  // [TreeCtrl.md#leading-separator-restored]
    for (const wstring& i: strs) {
        path += i;
        wxTreeItemId parentId = id;
        auto optid=tree.id(path);
        if (optid == nullopt) {
            id = AppendItem(id, i, image, image, new TreeItemData(i));
            if (&i != &strs.back()) {
                tree.addNode(parentId, id, path);
                SetItemImage(id, TreeCtrlIcon_FolderOpened, wxTreeItemIcon_Normal);
            }
            else {
                tree.addLeaf(parentId, id, path, nullptr);
                if (syncType == SyncItemType::File) {
                    SetItemImage(id, TreeCtrlIcon_File, wxTreeItemIcon_Normal);
                    SetItemImage(id, TreeCtrlIcon_FileSelected, wxTreeItemIcon_Selected);
                }
                else {
                    SetItemImage(id, TreeCtrlIcon_Folder, wxTreeItemIcon_Normal);
                    SetItemImage(id, TreeCtrlIcon_FolderSelected, wxTreeItemIcon_Selected);
                    SetItemImage(id, TreeCtrlIcon_FolderOpened, wxTreeItemIcon_Expanded);
                }
            }
        }
        else {
            id = optid.value();
        }
        path += separator;
    }
    return id;
}

void TreeCtrl::update()
{
    DeleteAllItems();
    AddRootItemToTree();
}

void TreeCtrl::ExpandNode(wxTreeItemId node)
{
    if (node != GetRootItem())
        Expand(node);
    wxTreeItemIdValue cookie;
    wxTreeItemId child = GetFirstChild(node, cookie);
    while (child.IsOk())
    {
        ExpandNode(child);
        child = GetNextChild(node, cookie);
    }
}

bool TreeCtrl::mouseClicked() 
{ 
    static int requestcout = 0;
    bool retval = mouseClicked_;
    if (mouseClicked_)
        ++requestcout;
    if (requestcout==2) {  // there are always 2 instances of TreeCtrl asked about mouse clicked
        mouseClicked_ = false;
        requestcout = 0;
    }
    return retval; 
}
