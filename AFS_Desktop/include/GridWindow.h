#pragma once

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#include <wx/treebase.h>
#endif

#include <string>

class GridCtrl;

class GridWindow: public wxScrolledWindow {
public:
    GridWindow(wxWindow* parent);
    GridWindow() {}
    std::wstring getSelected() const;
    void update();

    void OnContextMenu(wxContextMenuEvent& event);
    void OnSize(wxSizeEvent& event);
private:
    void ShowContextMenu(const wxPoint& pos);

    GridCtrl* gridCtrl_ = nullptr;
    wxDECLARE_NO_COPY_CLASS(GridWindow);
    wxDECLARE_EVENT_TABLE();
};


