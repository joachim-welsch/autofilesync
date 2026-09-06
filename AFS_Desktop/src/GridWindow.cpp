#include "GridWindow.h"

#include "utilwx.h"
#include "id.h"
#include "GridCtrl.h"

using utilwx::tul;
using std::wstring;

wxBEGIN_EVENT_TABLE(GridWindow, wxScrolledWindow)
EVT_CONTEXT_MENU(GridWindow::OnContextMenu)
EVT_SIZE(GridWindow::OnSize)
wxEND_EVENT_TABLE()

GridWindow::GridWindow(wxWindow* parent)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxHSCROLL | wxVSCROLL)
{
	gridCtrl_ = new GridCtrl(this, tul(id::GridCtrl), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* const sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(gridCtrl_, wxSizerFlags(2).Expand().Expand());
	SetSizer(sizer);
	SetClientSize(GetBestSize());
	gridCtrl_->addDataItems();
}

wstring GridWindow::getSelected() const
{
	return gridCtrl_->getSelected();
}

void GridWindow::update()
{
	gridCtrl_->update();
	SetClientSize(GetBestSize());
}

void GridWindow::ShowContextMenu(const wxPoint& pos)
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

void GridWindow::OnContextMenu(wxContextMenuEvent& event)
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

void GridWindow::OnSize(wxSizeEvent& event)
{
	if (gridCtrl_) {
		gridCtrl_->adjustColumnWidths(event.GetSize().GetWidth());
	}
	event.Skip();
}
