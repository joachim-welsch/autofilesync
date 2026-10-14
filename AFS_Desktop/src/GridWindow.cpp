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
#include "GridWindow.h"

#include "utilwx.h"
#include "id.h"
#include "GridCtrl.h"
#include "Translation.h"

using utilwx::tul;
using std::wstring;

wxBEGIN_EVENT_TABLE(GridWindow, wxScrolledWindow)
EVT_CONTEXT_MENU(GridWindow::on_context_menu)
EVT_SIZE(GridWindow::on_size)
wxEND_EVENT_TABLE()

GridWindow::GridWindow(wxWindow* parent)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxHSCROLL | wxVSCROLL)
{
	grid_ctrl_ = new GridCtrl(this, tul(id::GridCtrl), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* const sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(grid_ctrl_, wxSizerFlags(2).Expand().Expand());
	SetSizer(sizer);
	SetClientSize(GetBestSize());
	grid_ctrl_->add_data_items();
}

wstring GridWindow::get_selected() const
{
	return grid_ctrl_->get_selected();
}

void GridWindow::update()
{
	grid_ctrl_->update();
	SetClientSize(GetBestSize());
}

void GridWindow::show_context_menu(const wxPoint& pos)
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

void GridWindow::on_context_menu(wxContextMenuEvent& event)
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

void GridWindow::on_size(wxSizeEvent& event)
{
	if (grid_ctrl_) {
		grid_ctrl_->adjust_column_widths(event.GetSize().GetWidth());
	}
	event.Skip();
}
