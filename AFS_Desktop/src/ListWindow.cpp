#include "ListWindow.h"
#include "utilwx.h"
#include "id.h"
#include "ListCtrl.h"

using utilwx::tul;
using std::wstring;

wxBEGIN_EVENT_TABLE(ListWindow, wxScrolledWindow)
EVT_CONTEXT_MENU(ListWindow::OnContextMenu)
wxEND_EVENT_TABLE()

ListWindow::ListWindow(wxWindow* parent)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxHSCROLL | wxVSCROLL)
{
	listCtrl_ = new ListCtrl(this, tul(id::ListCtrl), wxDefaultPosition, wxDefaultSize, wxLC_LIST | wxBORDER_THEME);
	wxBoxSizer* const sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(listCtrl_, wxSizerFlags(2).Expand().Border());
	SetSizer(sizer);
	SetClientSize(GetBestSize());
	listCtrl_->addDataItems();
}

wstring ListWindow::getSelected() const
{
	return listCtrl_->getSelected();
}

void ListWindow::update()
{
	listCtrl_->update();
	SetClientSize(GetBestSize());
}

void ListWindow::ShowContextMenu(const wxPoint& pos)
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

void ListWindow::OnContextMenu(wxContextMenuEvent& event)
{
	wxPoint point = event.GetPosition();
	// If from keyboard
	if (point.x == -1 && point.y == -1) {
		wxSize size = GetSize();
		point.x = size.x / 2;
		point.y = size.y / 2;
	}
	else {
		point = ScreenToClient(point);
	}
	ShowContextMenu(point);
}
