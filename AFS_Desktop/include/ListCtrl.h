#pragma once
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#endif
#include "wx/listctrl.h"

class ListCtrl: public wxListCtrl {
public:
    ListCtrl(wxWindow* parent,
        const wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        long style)
        : wxListCtrl(parent, id, pos, size, style)
    {}

    void addDataItems();
    std::wstring getSelected() const;
    void update();
private:
    void OnDClick(wxMouseEvent& evt);
    wxDECLARE_EVENT_TABLE();
};

