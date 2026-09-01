#pragma once
#include "wx/wxprec.h"


#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/taskbar.h"

class MainWindow;

class TaskBarIcon: public wxTaskBarIcon {
public:
    TaskBarIcon(MainWindow* mwd) : mainWindow_(mwd) {}
    void OnLeftButtonDClick(wxTaskBarIconEvent&);
    void OnMenuRestore(wxCommandEvent&);
    void OnMenuExit(wxCommandEvent&);
    void OnMenuSetNewIcon(wxCommandEvent&);
    void OnMenuCheckmark(wxCommandEvent&);
    void OnMenuUICheckmark(wxUpdateUIEvent&);
    void OnMenuSub(wxCommandEvent&);
    virtual wxMenu* CreatePopupMenu() wxOVERRIDE;
private:
    MainWindow* mainWindow_ = nullptr;
    wxDECLARE_EVENT_TABLE();

};

