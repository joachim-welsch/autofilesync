#include "GridCtrl.h"

#include "Config.h"
#include "SyncItemDialog.h"
#include "TextResources.h"

using namespace AFS_Core;

using std::wstring;
using std::nullopt;

namespace {
	// Untergrenze fuer die Name-Spalte (Zeilenbeschriftung), damit sie bei
	// kurzen Namen nicht unnoetig schmal wird; laengere Namen weiten sie per
	// wxGRID_AUTOSIZE in addDataItems() darueber hinaus auf.
	constexpr int kMinRowLabelSize = 150;
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

// And a custom attributes provider which uses custom column header renderer
// defined above
class CustomColumnHeadersProvider : public wxGridCellAttrProvider
{
public:
	// by default custom column renderer is not used, call
	// UseCustomColHeaders() to enable it
	CustomColumnHeadersProvider()
		: m_customOddRenderer(*wxYELLOW, *wxBLUE),
		m_customEvenRenderer(*wxWHITE, *wxBLACK),
		m_useCustom(false)
	{
	}

	// enable or disable the use of custom renderer for column headers
	void UseCustomColHeaders(bool use = true) { m_useCustom = use; }

protected:
	virtual const wxGridColumnHeaderRenderer& GetColumnHeaderRenderer(int col) wxOVERRIDE
	{
		// if enabled, use custom renderers
		if (m_useCustom)
		{
			// and use different ones for odd and even columns -- just to show
			// that we can
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
	// wxGrid centres row labels by default; a name longer than the column
	// then gets clipped on both sides instead of staying readable from the
	// start.
	SetRowLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);
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
	// Name-Spalte an den laengsten aktuell angezeigten Namen anpassen, damit
	// er immer vollstaendig sichtbar bleibt statt abgeschnitten zu werden.
	SetRowLabelSize(wxGRID_AUTOSIZE);
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
	// Rebuilding rows from scratch drops any selection/cursor position;
	// restore it by name so editing (or deleting) an item doesn't force the
	// user to re-click it before they can edit/delete it again.
	const wstring selectedName = getSelected();
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
		found = Config::instance().findByName(name.ToStdWstring());
	if (!found.has_value()) // click on a row label with no matching item
		return;
	// found is a copy; write it back under Config's lock via the original name
	// (the dialog may have renamed it).
	const std::wstring originalName = found->name();
	SyncItemDialog dlg(&*found, this, wxID_ANY, txt::EditSynchronisation);
	if (dlg.ShowModal() == wxID_OK) {
		dlg.saveData(*found);
		Config::instance().updateByName(originalName, *found);
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
	auto found = Config::instance().findByName(name.ToStdWstring());
	if (found != std::nullopt) {
		// SyncItemDialog disables its OK button while source/target are empty
		// (onUpdateOkButton); the grid's inline cell edit has no such gate, so
		// it must reject the same case explicitly instead of silently saving it.
		if ((col == 1 || col == 2) && value.IsEmpty()) {
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
		// found is a copy; write it back under Config's lock (also sets changed).
		cfg.updateByName(name.ToStdWstring(), *found);
	}
	ev.Skip();
}

void GridCtrl::adjustColumnWidths(int windowWidth)
{
	// Calculate available width (subtract row label width and scrollbar)
	int availableWidth = windowWidth - GetRowLabelSize() - 20;
	if (availableWidth < 100) return;
	
	// Column 0 (Aktiv) has fixed width
	const int aktivWidth = 50;
	// Remaining width is split between Source and Target columns
	int remainingWidth = availableWidth - aktivWidth;
	int colWidth = remainingWidth / 2;
	
	SetColSize(0, aktivWidth);
	SetColSize(1, colWidth);
	SetColSize(2, colWidth);
}
