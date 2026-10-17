/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "ListWindow.h"
#include "utilwx.h"
#include "id.h"
#include "ListCtrl.h"
#include "Translation.h"

using utilwx::tul;
using std::wstring;

wxBEGIN_EVENT_TABLE(ListWindow, wxScrolledWindow)
EVT_CONTEXT_MENU(ListWindow::on_context_menu)
wxEND_EVENT_TABLE()

ListWindow::ListWindow(wxWindow* parent)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxHSCROLL | wxVSCROLL)
{
	list_ctrl_ = new ListCtrl(this, tul(id::ListCtrl), wxDefaultPosition, wxDefaultSize, wxLC_LIST | wxBORDER_THEME);
	wxBoxSizer* const sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(list_ctrl_, wxSizerFlags(2).Expand().Border());
	SetSizer(sizer);
	SetClientSize(GetBestSize());
	list_ctrl_->add_data_items();
}

wstring ListWindow::get_selected() const
{
	return list_ctrl_->get_selected();
}

void ListWindow::update()
{
	list_ctrl_->update();
	SetClientSize(GetBestSize());
}

void ListWindow::show_context_menu(const wxPoint& pos)
{
	wxMenu menu;
	if (wxGetKeyState(WXK_SHIFT)) {
	}
	else {
		menu.Append(tul(id::EditSync), AFS_Core::translation::translate("context_edit"));
		menu.Append(tul(id::DeleteSync), AFS_Core::translation::translate("context_delete"));
		PopupMenu(&menu, pos);
	}
}

void ListWindow::on_context_menu(wxContextMenuEvent& event)
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
	show_context_menu(point);
}
