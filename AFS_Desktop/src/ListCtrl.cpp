#include "ListCtrl.h"
#include "Config.h"
#include "SyncItemDialog.h"
#include "TextResources.h"

using namespace AFS_Core;

using std::wstring;
using std::nullopt;

wxBEGIN_EVENT_TABLE(ListCtrl, wxListCtrl)
EVT_LEFT_DCLICK(ListCtrl::OnDClick)
wxEND_EVENT_TABLE()


void ListCtrl::addDataItems()
{
    Config& cfg = Config::instance();
    long index = 0;
    for (const auto& i : cfg.syncitems()) {
        InsertItem(index, i.name());
        index++;
    }

}

wstring ListCtrl::getSelected() const
{
	const long sel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (sel != wxNOT_FOUND) {
		return GetItemText(sel).ToStdWstring();
	}
	else
		return L"";
}

void ListCtrl::update()
{
	// Rebuilding items from scratch drops any selection; restore it by name
	// so editing an item doesn't force the user to re-click it before they
	// can edit/delete it again.
	const wstring selectedName = getSelected();
	ClearAll();
	addDataItems();
	if (!selectedName.empty()) {
		for (long i = 0; i < GetItemCount(); ++i) {
			if (GetItemText(i).ToStdWstring() == selectedName) {
				SetItemState(i, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
				EnsureVisible(i);
				break;
			}
		}
	}
}

void ListCtrl::OnDClick(wxMouseEvent&)
{
	const wstring syncName = getSelected();
	std::optional<SyncItem> found = nullopt;
	if (!syncName.empty())
		found = Config::instance().find_by_name(syncName);
	if (!found.has_value()) // double-click on empty list area, or item no longer exists
		return;
	// found is a copy; write it back under Config's lock via the original name
	// (the dialog may have renamed it).
	const std::wstring originalName = found->name();
	SyncItemDialog dlg(&*found, this, wxID_ANY, txt::EditSynchronisation);
	if (dlg.ShowModal() == wxID_OK) {
		dlg.saveData(*found);
		Config::instance().update_by_name(originalName, *found);
		update();
	}
}