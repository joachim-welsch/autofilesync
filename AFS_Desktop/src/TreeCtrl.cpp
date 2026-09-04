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
    //CreateStateImageList(false);
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
    //int image = 0; // wxGetApp().ShowImages() ? MyTreeCtrl::TreeCtrlIcon_Folder : -1;
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

    // Make an image list containing small icons
    wxImageList* images = new wxImageList(size, size, true);

    // should correspond to TreeCtrlIcon_xxx enum
    //wxBusyCursor wait;
    wxIcon icons[5];

    //if (m_alternateImages)
    //{
        icons[TreeCtrlIcon_File] = wxIcon(icon1_xpm);
        icons[TreeCtrlIcon_FileSelected] = wxIcon(icon2_xpm);
        icons[TreeCtrlIcon_Folder] = wxIcon(icon3_xpm);
        icons[TreeCtrlIcon_FolderSelected] = wxIcon(icon4_xpm);
        icons[TreeCtrlIcon_FolderOpened] = wxIcon(icon5_xpm);
    //}
    //else
    //{
    //    wxSize iconSize(size, size);

    //    icons[TreeCtrlIcon_File] =
    //        icons[TreeCtrlIcon_FileSelected] = wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_LIST, iconSize);
    //    icons[TreeCtrlIcon_Folder] =
    //        icons[TreeCtrlIcon_FolderSelected] =
    //        icons[TreeCtrlIcon_FolderOpened] = wxArtProvider::GetIcon(wxART_FOLDER, wxART_LIST, iconSize);
    //}
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
    //if (del)
    //{
    //    SetStateImageList(NULL);
    //    return;
    //}

    //wxImageList* states;
    //wxBusyCursor wait;

    //if (m_alternateStates)
    //{
    //    wxIcon icons[5];
    //    icons[0] = wxIcon(state1_xpm);  // yellow
    //    icons[1] = wxIcon(state2_xpm);  // green
    //    icons[2] = wxIcon(state3_xpm);  // red
    //    icons[3] = wxIcon(state4_xpm);  // blue
    //    icons[4] = wxIcon(state5_xpm);  // black

    //    int width = icons[0].GetWidth(),
    //        height = icons[0].GetHeight();

    //    // Make an state image list containing small icons
    //    states = new wxImageList(width, height, true);

    //    for (size_t i = 0; i < WXSIZEOF(icons); i++)
    //        states->Add(icons[i]);
    //}
    //else
    //{
    //    wxIcon icons[2];
    //    icons[0] = wxIcon(unchecked_xpm);
    //    icons[1] = wxIcon(checked_xpm);
    //    int width = icons[0].GetWidth(),
    //        height = icons[0].GetHeight();

    //    // Make an state image list containing small icons
    //    states = new wxImageList(width, height, true);

    //    for (size_t i = 0; i < WXSIZEOF(icons); i++)
    //        states->Add(icons[i]);
    //}
    //AssignStateImageList(states);
}

wxTreeItemId TreeCtrl::rootId() const
{
    return rootId_;
}

wxTreeItemId TreeCtrl::addBranch(const wstring& l, Tree& tree, SyncItemType syncType)
{
    vector<wstring> strs;
    // SSH paths ("account:/remote/...") and local Unix/macOS paths use '/'
    // exclusively and never contain '\'; only Windows local paths do. Detect
    // from the actual path instead of hardcoding '\', or every non-Windows
    // path shows up as a single flat leaf instead of a folder hierarchy.
    const bool backslashPath = l.find(L'\\') != wstring::npos;
    boost::split(strs, l, boost::is_any_of(backslashPath ? "\\" : "/"));
    // A leading/trailing/doubled separator (e.g. a Dir item's path ending in
    // '/') otherwise produces an empty segment, showing up as a spurious
    // unnamed node in the tree.
    strs.erase(std::remove_if(strs.begin(), strs.end(), [](const wstring& s) { return s.empty(); }), strs.end());
    int image = 0;
    wxTreeItemId id = rootId_;
    const wchar_t separator = backslashPath ? L'\\' : L'/';
    // A leaf's registered path must match the SyncItem's source()/target() verbatim
    // (Config::find() compares them for string equality to enable the tree view's
    // context menu), so a leading separator stripped from strs above has to be put
    // back here - only the empty segments it produced were the problem.
    wstring path = l.substr(0, l.find_first_not_of(separator));
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
