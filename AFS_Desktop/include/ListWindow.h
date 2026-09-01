#pragma once
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#include <wx/treebase.h>
#endif

#include <string>

class ListCtrl;

class ListWindow: public wxScrolledWindow {
public:
	ListWindow(wxWindow* parent);
    ListWindow() {}
    std::wstring getSelected() const;
    void update();
    
    void OnContextMenu(wxContextMenuEvent& event);
private:
    void ShowContextMenu(const wxPoint& pos);

    ListCtrl* listCtrl_ = nullptr;
    wxDECLARE_NO_COPY_CLASS(ListWindow);
    wxDECLARE_EVENT_TABLE();
};

