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

