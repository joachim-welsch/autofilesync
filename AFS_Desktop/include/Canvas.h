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
    // Selects and scrolls to path's node if this side's tree has one for it.
    // Returns false (and leaves the selection untouched) if it doesn't.
    bool selectPath(const std::wstring& path);
    // A left click on item: if it is a SyncItem's own source/target root,
    // reveal the matching path on the other side instead of just clearing
    // its selection - the two sides then always point at the same SyncItem,
    // which is also what MainWindow::findSyncItem() relies on.
    void onItemLeftClicked(wxTreeItemId item);
protected:
    wxStaticText* heading_ = nullptr;
    TreeCtrl* treeCtrl_ = nullptr;
    Tree tree_;
    Canvas* other_ = nullptr;
private:
    void ShowContextMenu(const wxPoint& pos);
    void OnRightUp(wxMouseEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    // Lays heading_ and treeCtrl_ out top-to-bottom. Bound to EVT_SIZE rather
    // than done in an overridden SetSize(): the splitter window resizes its
    // panes through its own internal layout, not through calls this class
    // would see, and GetClientSize() only reports the final size once the
    // toolkit's own resize has actually settled.
    void OnSize(wxSizeEvent& event);

    wxDECLARE_NO_COPY_CLASS(Canvas);
    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_SCROLLED_WIN = wxID_HIGHEST + 1,
    ID_SRC_TREE_CTRL
};
