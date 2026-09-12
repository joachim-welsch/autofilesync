#include "TaskBarIcon.h"
#include "MainWindow.h"
#include "../icons/smile.xpm"

enum
{
    PU_RESTORE = 10001,
    PU_EXIT,
    PU_CHECKMARK,
    PU_SUBMAIN
};

wxBEGIN_EVENT_TABLE(TaskBarIcon, wxTaskBarIcon)
EVT_MENU(PU_RESTORE, TaskBarIcon::OnMenuRestore)
EVT_MENU(PU_EXIT, TaskBarIcon::OnMenuExit)
EVT_MENU(PU_CHECKMARK, TaskBarIcon::OnMenuCheckmark)
EVT_UPDATE_UI(PU_CHECKMARK, TaskBarIcon::OnMenuUICheckmark)
EVT_TASKBAR_LEFT_DCLICK(TaskBarIcon::OnLeftButtonDClick)
wxEND_EVENT_TABLE()

void TaskBarIcon::OnMenuRestore(wxCommandEvent&)
{
    if (mainWindow_ != nullptr)
        mainWindow_->Show(true);
}

void TaskBarIcon::OnMenuExit(wxCommandEvent&)
{
    if (mainWindow_ != nullptr)
        mainWindow_->Close(true);
}

static bool check = true;

void TaskBarIcon::OnMenuCheckmark(wxCommandEvent&)
{
    check = !check;
}

void TaskBarIcon::OnMenuUICheckmark(wxUpdateUIEvent& event)
{
    event.Check(check);
}

void TaskBarIcon::OnMenuSetNewIcon(wxCommandEvent&)
{
    wxIcon icon(smile_xpm);

    if (!SetIcon(icon, "wxTaskBarIcon Sample - a different icon"))
        wxMessageBox("Could not set new icon.");
}

void TaskBarIcon::OnMenuSub(wxCommandEvent&)
{
    wxMessageBox("You clicked on a submenu!");
}

wxMenu* TaskBarIcon::CreatePopupMenu()
{
    wxMenu* menu = new wxMenu;
    menu->Append(PU_RESTORE, "&Hauptfenster wieder herstellen");
#ifdef __WXOSX__  // OSX has a built-in quit menu for the dock menu, but not for the status item
    if (OSXIsStatusItem())
#endif
    {
        menu->AppendSeparator();
        menu->Append(PU_EXIT, "Ver&lassen");
    }
    return menu;
}

void TaskBarIcon::OnLeftButtonDClick(wxTaskBarIconEvent&)
{
    if (mainWindow_ != nullptr)
        mainWindow_->Show(true);
}
