#include "Canvas.h"
#include "TreeCtrl.h"
#include "utilwx.h"
#include "id.h"
#include "Config.h"

using namespace AFS_Core;

using utilwx::tul;
using std::wstring;
using std::nullopt;

wxBEGIN_EVENT_TABLE(Canvas, wxScrolledWindow)
    EVT_CONTEXT_MENU(Canvas::OnContextMenu)
    EVT_SIZE(Canvas::OnSize)
wxEND_EVENT_TABLE()

Canvas::Canvas(wxWindow* parent, const wxString& heading)
 : wxScrolledWindow(parent, ID_SCROLLED_WIN, wxDefaultPosition, wxDefaultSize,
	 wxHSCROLL | wxVSCROLL)
    , other_(nullptr)
{
	heading_ = new wxStaticText(this, wxID_ANY, heading, wxDefaultPosition, wxDefaultSize,
		wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	heading_->SetFont(heading_->GetFont().Bold());
	long style = wxTR_DEFAULT_STYLE | wxTR_HAS_VARIABLE_ROW_HEIGHT | wxSUNKEN_BORDER;  // [Canvas.md#no-inline-rename]
    treeCtrl_ = new TreeCtrl(this, ID_SRC_TREE_CTRL, wxDefaultPosition, wxDefaultSize, style);
}

void Canvas::OnSize(wxSizeEvent& event)
{
    const wxSize clientSize = GetClientSize();
    const int headingHeight = heading_->GetBestSize().GetHeight() + 4;
    heading_->SetSize(0, 0, clientSize.GetWidth(), headingHeight);
    treeCtrl_->SetSize(0, headingHeight, clientSize.GetWidth(), clientSize.GetHeight() - headingHeight);
    event.Skip();
}

wstring Canvas::selectedPath() const
{
    auto selItem = treeCtrl_->GetSelection();
    return tree_.path(selItem);
}

void Canvas::ShowContextMenu(const wxPoint& pos)
{
    wxMenu menu;
    if (wxGetKeyState(WXK_SHIFT)) {
    }
    else {
        menu.Append(tul(id::EditSync), "&Editiere");
        menu.Append(tul(id::DeleteSync), L"&Lösche");
        PopupMenu(&menu, pos);
    }
}

void Canvas::OnContextMenu(wxContextMenuEvent& event)
{
    wxPoint point = event.GetPosition();
    if (point.x == -1 && point.y == -1) {  // (-1,-1) means the menu came from the keyboard
        wxSize size = GetSize();
        point.x = size.x / 2;
        point.y = size.y / 2;
    }
    else {
        point = ScreenToClient(point);
    }
    ShowContextMenu(point);
}

void Canvas::OnRightUp(wxMouseEvent& event)
{
    ShowContextMenu(event.GetPosition());
}

bool Canvas::mouseClicked() const
{
    return treeCtrl_->mouseClicked();
}

void Canvas::deSelect()
{
    auto selItem=treeCtrl_->GetSelection();
    if (selItem != nullptr) {
        treeCtrl_->SelectItem(selItem, false);
        treeCtrl_->ClearFocusedItem();
    }
}

bool Canvas::selectPath(const wstring& path)
{
    auto id = tree_.id(path);
    if (id == nullopt)
        return false;
    treeCtrl_->SelectItem(id.value());
    treeCtrl_->EnsureVisible(id.value());
    return true;
}

void Canvas::onItemLeftClicked(wxTreeItemId item)
{
    if (item.IsOk()) {
        auto found = Config::instance().find(tree_.path(item), source());
        if (found.has_value() && other()->selectPath(source() ? found->target() : found->source()))
            return;
    }
    other()->deSelect();
}
