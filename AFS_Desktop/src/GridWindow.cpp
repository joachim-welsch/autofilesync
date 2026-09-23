/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
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

void GridWindow::OnSize(wxSizeEvent& event)
{
	if (gridCtrl_) {
		gridCtrl_->adjustColumnWidths(event.GetSize().GetWidth());
	}
	event.Skip();
}
