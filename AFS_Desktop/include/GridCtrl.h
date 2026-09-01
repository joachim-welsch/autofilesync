#pragma once
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#endif
#include "wx/grid.h"

class GridCtrl: public wxGrid {
public:
    GridCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size);

    void addDataItems();
    std::wstring getSelected() const;
    void update();
    void adjustColumnWidths(int windowWidth);
private:
    void OnCellValueChanged(wxGridEvent&);
    void OnModifyEntry(wxGridEvent&);
    wxDECLARE_EVENT_TABLE();
};

