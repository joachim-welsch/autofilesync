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
#include "GridCtrl.h"

#include "AutoSave.h"
#include "Config.h"
#include "SyncItemDialog.h"
#include "TextResources.h"
#include "Translation.h"

using namespace AFS_Core;

using std::wstring;
using std::nullopt;

namespace {
	constexpr int MIN_ROW_LABEL_SIZE = 150;  // [GridCtrl.md#MIN_ROW_LABEL_SIZE]
}

wxBEGIN_EVENT_TABLE(GridCtrl, wxGrid)
EVT_GRID_LABEL_LEFT_CLICK(GridCtrl::on_modify_entry)
EVT_GRID_CELL_CHANGED(GridCtrl::on_cell_value_changed)
wxEND_EVENT_TABLE()

GridCtrl::GridCtrl(wxWindow* parent,
	const wxWindowID id,
	const wxPoint& pos,
	const wxSize& size)
	: wxGrid(parent, id, pos, size)
{
	CreateGrid(0, 0);
	AppendCols(3);
	SetCornerLabelValue(translation::translate("name"));
	SetColLabelValue(0, translation::translate("active"));
	SetColLabelValue(1, translation::translate("source"));
	SetColLabelValue(2, translation::translate("target"));
	SetColSize(0, FromDIP(40));
	SetColSize(1, FromDIP(200));
	SetColSize(2, FromDIP(200));
	SetColLabelSize(FromDIP(25));
	SetRowLabelSize(FromDIP(MIN_ROW_LABEL_SIZE));
	SetRowLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);  // [GridCtrl.md#row-label-alignment]
}


void GridCtrl::add_data_items()
{
	Config& cfg = Config::instance();
	int index = 0;
	for (const auto& i : cfg.syncitems()) {
		AppendRows(1);
		SetRowLabelValue(index, i.name());
		SetCellRenderer(index, 0, new wxGridCellBoolRenderer);
		SetCellEditor(index, 0, new wxGridCellBoolEditor);
		SetCellValue(index, 0, i.enabled() ? L"1" : L"");

		SetCellValue(index, 1, i.source());
		SetCellValue(index, 2, i.target());

		index++;
	}
	SetRowLabelSize(wxGRID_AUTOSIZE);  // [GridCtrl.md#autosize-row-label]
	if (GetRowLabelSize() < FromDIP(MIN_ROW_LABEL_SIZE))
		SetRowLabelSize(FromDIP(MIN_ROW_LABEL_SIZE));
	adjust_column_widths(GetSize().GetWidth());
}

wstring GridCtrl::get_selected() const
{
	int row = GetGridCursorRow();
	if (row < 0 || row >= GetNumberRows())
		return L"";
	return GetRowLabelValue(row).ToStdWstring();
}

void GridCtrl::update()
{
	const wstring selected_name = get_selected();  // [GridCtrl.md#selection-restored]
	DeleteRows(0, GetNumberRows());
	add_data_items();
	if (!selected_name.empty()) {
		for (int row = 0; row < GetNumberRows(); ++row) {
			if (GetRowLabelValue(row).ToStdWstring() == selected_name) {
				SetGridCursor(row, 0);
				SelectRow(row);
				break;
			}
		}
	}
}

void GridCtrl::on_modify_entry(wxGridEvent& ev)
{
	int row = ev.GetRow();
	if (row < 0)
		return;
	auto name = GetRowLabelValue(row);
	std::optional<SyncItem> found = nullopt;
	if (!name.empty())
		found = Config::instance().find_by_name(name.ToStdWstring());
	if (!found.has_value()) // click on a row label with no matching item
		return;
	const std::wstring original_name = found->name();  // [GridCtrl.md#copy-written-back]
	SyncItemDialog dlg(&*found, this, wxID_ANY, txt::edit_synchronisation());
	if (dlg.ShowModal() == wxID_OK) {
		dlg.save_data(*found);
		Config::instance().update_by_name(original_name, *found);
		autosave::after_change(this);  // [AutoSave.md#after_change]
		update();
	}
}

void GridCtrl::on_cell_value_changed(wxGridEvent& ev)
{
	int row = ev.GetRow(),
		col = ev.GetCol();
	auto value = GetCellValue(row, col);
	auto name = GetRowLabelValue(row);
	auto& cfg = Config::instance();
	auto found = Config::instance().find_by_name(name.ToStdWstring());
	if (found != std::nullopt) {
		if ((col == 1 || col == 2) && value.IsEmpty()) {  // [GridCtrl.md#empty-source-or-target-rejected]
			wxMessageBox(translation::translate("source_target_required"), translation::translate("error"), wxOK | wxICON_WARNING, this);
			SetCellValue(row, col, col == 1 ? found->source() : found->target());
			ev.Skip();
			return;
		}
		switch (col) {
		case 0: found->enabled(value==L"1"); break;
		case 1:	found->source(value.ToStdWstring()); break;
		case 2:	found->target(value.ToStdWstring()); break;
		}
		cfg.update_by_name(name.ToStdWstring(), *found);  // [GridCtrl.md#copy-written-back]
		autosave::after_change(this);
	}
	ev.Skip();
}

void GridCtrl::adjust_column_widths(int window_width)
{
	int available_width = window_width - GetRowLabelSize() - 20;
	if (available_width < 100) return;
	
	const int aktiv_width = 50;
	int remaining_width = available_width - aktiv_width;
	int col_width = remaining_width / 2;
	
	SetColSize(0, aktiv_width);
	SetColSize(1, col_width);
	SetColSize(2, col_width);
}
