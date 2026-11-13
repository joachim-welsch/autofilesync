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
#include "TreeCtrl.h"
#include "wx/artprov.h"
#include "../icons/icon1.xpm"
#include "../icons/icon2.xpm"
#include "../icons/icon3.xpm"
#include "../icons/icon4.xpm"
#include "../icons/icon5.xpm"

#include "TreeItemData.h"
#include "Config.h"
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "Tree.h"
#include "Translation.h"
#include "Canvas.h"

using namespace AFS_Core;

using std::wstring;
using std::vector;
using std::nullopt;

wxBEGIN_EVENT_TABLE(TreeCtrl, wxTreeCtrl)
    EVT_LEFT_DOWN(TreeCtrl::on_mouse_left_down)
wxEND_EVENT_TABLE()

TreeCtrl::TreeCtrl(Canvas* parent, const wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style)
    : wxTreeCtrl(parent, id, pos, size, style)
    , parent_(parent)
{
    create_image_list();
    add_root_item_to_tree();
}

void TreeCtrl::on_mouse_left_down(wxMouseEvent& evt)
{
    wxPoint position = evt.GetPosition();
    int flags = 0;
    parent_->on_item_left_clicked(HitTest(position, flags));
    if (position.x >= 0 && position.y >= 0) {
        mouse_clicked_ = true;
    }
    evt.Skip();
}

void TreeCtrl::add_root_item_to_tree()
{
    const wxString label = translation::translate("tree_root");
    root_id_ = AddRoot(label, TreeCtrlIcon_Folder, TreeCtrlIcon_Folder, new TreeItemData(label));
    if (!HasFlag(wxTR_HIDE_ROOT) /* && image != -1*/)
    {
        SetItemImage(root_id_, TreeCtrlIcon_FolderOpened, wxTreeItemIcon_Expanded);
    }
}


void TreeCtrl::create_image_list(int size)
{
    if (size == -1)
    {
        SetImageList(NULL);
        return;
    }
    if (size == 0)
        size = image_size_;
    else
        image_size_ = size;

    wxImageList* images = new wxImageList(size, size, true);

    wxIcon icons[5];  // should correspond to the TreeCtrlIcon_xxx enum
    icons[TreeCtrlIcon_File] = wxIcon(icon1_xpm);
    icons[TreeCtrlIcon_FileSelected] = wxIcon(icon2_xpm);
    icons[TreeCtrlIcon_Folder] = wxIcon(icon3_xpm);
    icons[TreeCtrlIcon_FolderSelected] = wxIcon(icon4_xpm);
    icons[TreeCtrlIcon_FolderOpened] = wxIcon(icon5_xpm);
    for (size_t i = 0; i < WXSIZEOF(icons); i++)
    {
        int size_orig = icons[0].GetWidth();
        if (size == size_orig)
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

void TreeCtrl::create_state_image_list(bool /*del*/)
{
}

wxTreeItemId TreeCtrl::root_id() const
{
    return root_id_;
}

wxTreeItemId TreeCtrl::add_branch(const wstring& l, Tree& tree, SyncItemType sync_type)
{
    vector<wstring> strs;
    const bool backslash_path = l.find(L'\\') != wstring::npos;  // [TreeCtrl.md#separator-detected-from-path]
    boost::split(strs, l, boost::is_any_of(backslash_path ? "\\" : "/"));
    strs.erase(std::remove_if(strs.begin(), strs.end(), [](const wstring& s) { return s.empty(); }), strs.end());  // [TreeCtrl.md#empty-segments-removed]
    int image = 0;
    wxTreeItemId id = root_id_;
    const wchar_t separator = backslash_path ? L'\\' : L'/';
    wstring path = l.substr(0, l.find_first_not_of(separator));  // [TreeCtrl.md#leading-separator-restored]
    for (const wstring& i: strs) {
        path += i;
        wxTreeItemId parent_id = id;
        auto optid=tree.id(path);
        if (optid == nullopt) {
            id = AppendItem(id, i, image, image, new TreeItemData(i));
            if (&i != &strs.back()) {
                tree.add_node(parent_id, id, path);
                SetItemImage(id, TreeCtrlIcon_FolderOpened, wxTreeItemIcon_Normal);
            }
            else {
                tree.add_leaf(parent_id, id, path, nullptr);
                if (sync_type == SyncItemType::File) {
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
    add_root_item_to_tree();
}

void TreeCtrl::expand_node(wxTreeItemId node)
{
    if (node != GetRootItem())
        Expand(node);
    wxTreeItemIdValue cookie;
    wxTreeItemId child = GetFirstChild(node, cookie);
    while (child.IsOk())
    {
        expand_node(child);
        child = GetNextChild(node, cookie);
    }
}

bool TreeCtrl::mouse_clicked() 
{ 
    static int requestcout = 0;
    bool retval = mouse_clicked_;
    if (mouse_clicked_)
        ++requestcout;
    if (requestcout==2) {  // there are always 2 instances of TreeCtrl asked about mouse clicked
        mouse_clicked_ = false;
        requestcout = 0;
    }
    return retval; 
}
