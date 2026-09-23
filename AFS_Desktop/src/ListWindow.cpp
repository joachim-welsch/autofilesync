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
