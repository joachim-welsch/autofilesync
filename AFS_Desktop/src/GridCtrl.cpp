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

#include "Config.h"
#include "SyncItemDialog.h"
#include "TextResources.h"

using namespace AFS_Core;

using std::wstring;
using std::nullopt;

namespace {
	constexpr int kMinRowLabelSize = 150;  // [GridCtrl.md#kMinRowLabelSize]
}

wxBEGIN_EVENT_TABLE(GridCtrl, wxGrid)
EVT_GRID_LABEL_LEFT_CLICK(GridCtrl::OnModifyEntry)
EVT_GRID_CELL_CHANGED(GridCtrl::OnCellValueChanged)
wxEND_EVENT_TABLE()

class CustomColumnHeaderRenderer : public wxGridColumnHeaderRenderer
{
public:
	CustomColumnHeaderRenderer(const wxColour& colFg, const wxColour& colBg)
		: m_colFg(colFg),
		m_colBg(colBg)
	{}

	virtual void DrawLabel(const wxGrid& WXUNUSED(grid),
		wxDC& dc,
		const wxString& value,
		const wxRect& rect,
		int horizAlign,
		int vertAlign,
		int WXUNUSED(textOrientation)) const wxOVERRIDE
	{
		dc.SetTextForeground(m_colFg);
		dc.SetFont(wxITALIC_FONT->Bold());
		dc.DrawLabel(value, rect, horizAlign | vertAlign);
	}

	virtual void DrawBorder(const wxGrid& WXUNUSED(grid),
		wxDC& dc,
		wxRect& rect) const wxOVERRIDE
	{
		dc.SetPen(*wxTRANSPARENT_PEN);
		dc.SetBrush(wxBrush(m_colBg));
		dc.DrawRectangle(rect);
	}

private:
	const wxColour m_colFg, m_colBg;

	wxDECLARE_NO_COPY_CLASS(CustomColumnHeaderRenderer);
};

class CustomColumnHeadersProvider : public wxGridCellAttrProvider
{
public:
	CustomColumnHeadersProvider()  // [GridCtrl.md#CustomColumnHeadersProvider]
		: m_customOddRenderer(*wxYELLOW, *wxBLUE),
		m_customEvenRenderer(*wxWHITE, *wxBLACK),
		m_useCustom(false)
	{
	}

	void UseCustomColHeaders(bool use = true) { m_useCustom = use; }

protected:
	virtual const wxGridColumnHeaderRenderer& GetColumnHeaderRenderer(int col) wxOVERRIDE
	{
		if (m_useCustom)
		{
			return col % 2 ? m_customOddRenderer : m_customEvenRenderer;
		}

		return wxGridCellAttrProvider::GetColumnHeaderRenderer(col);
	}

private:
	CustomColumnHeaderRenderer m_customOddRenderer,
		m_customEvenRenderer;

	bool m_useCustom;

	wxDECLARE_NO_COPY_CLASS(CustomColumnHeadersProvider);
};

GridCtrl::GridCtrl(wxWindow* parent,
	const wxWindowID id,
	const wxPoint& pos,
	const wxSize& size)
	: wxGrid(parent, id, pos, size)
{
	CreateGrid(0, 0);
	GetTable()->SetAttrProvider(new CustomColumnHeadersProvider());
	AppendCols(3);
	SetCornerLabelValue("Name");
	SetColLabelValue(0, "Aktiv");
	SetColLabelValue(1, "Quelle");
	SetColLabelValue(2, "Ziel");
	SetColSize(0, 40);
	SetColSize(1, 200);
	SetColSize(2, 200);
	SetColLabelSize(25);
	SetRowLabelSize(kMinRowLabelSize);
	SetRowLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);  // [GridCtrl.md#row-label-alignment]
}


void GridCtrl::addDataItems()
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
	if (GetRowLabelSize() < kMinRowLabelSize)
		SetRowLabelSize(kMinRowLabelSize);
	adjustColumnWidths(GetSize().GetWidth());
}

wstring GridCtrl::getSelected() const
{
	int row = GetGridCursorRow();
	if (row < 0 || row >= GetNumberRows())
		return L"";
	return GetRowLabelValue(row).ToStdWstring();
}

void GridCtrl::update()
{
	const wstring selectedName = getSelected();  // [GridCtrl.md#selection-restored]
	DeleteRows(0, GetNumberRows());
	addDataItems();
	if (!selectedName.empty()) {
		for (int row = 0; row < GetNumberRows(); ++row) {
			if (GetRowLabelValue(row).ToStdWstring() == selectedName) {
				SetGridCursor(row, 0);
				SelectRow(row);
				break;
			}
		}
	}
}

void GridCtrl::OnModifyEntry(wxGridEvent& ev)
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
	const std::wstring originalName = found->name();  // [GridCtrl.md#copy-written-back]
	SyncItemDialog dlg(&*found, this, wxID_ANY, txt::EditSynchronisation);
	if (dlg.ShowModal() == wxID_OK) {
		dlg.saveData(*found);
		Config::instance().update_by_name(originalName, *found);
		update();
	}
}

void GridCtrl::OnCellValueChanged(wxGridEvent& ev)
{
	int row = ev.GetRow(),
		col = ev.GetCol();
	auto value = GetCellValue(row, col);
	auto name = GetRowLabelValue(row);
	auto& cfg = Config::instance();
	auto found = Config::instance().find_by_name(name.ToStdWstring());
	if (found != std::nullopt) {
		if ((col == 1 || col == 2) && value.IsEmpty()) {  // [GridCtrl.md#empty-source-or-target-rejected]
			wxMessageBox(_(L"Quelle und Ziel dürfen nicht leer sein."), _("Fehler"), wxOK | wxICON_WARNING, this);
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
	}
	ev.Skip();
}

void GridCtrl::adjustColumnWidths(int windowWidth)
{
	int availableWidth = windowWidth - GetRowLabelSize() - 20;
	if (availableWidth < 100) return;
	
	const int aktivWidth = 50;
	int remainingWidth = availableWidth - aktivWidth;
	int colWidth = remainingWidth / 2;
	
	SetColSize(0, aktivWidth);
	SetColSize(1, colWidth);
	SetColSize(2, colWidth);
}
