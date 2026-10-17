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
#include "ListCtrl.h"
#include "AutoSave.h"
#include "Config.h"
#include "SyncItemDialog.h"
#include "TextResources.h"

using namespace AFS_Core;

using std::wstring;
using std::nullopt;

wxBEGIN_EVENT_TABLE(ListCtrl, wxListCtrl)
EVT_LEFT_DCLICK(ListCtrl::on_double_click)
wxEND_EVENT_TABLE()


void ListCtrl::add_data_items()
{
    Config& cfg = Config::instance();
    long index = 0;
    for (const auto& i : cfg.syncitems()) {
        InsertItem(index, i.name());
        index++;
    }

}

wstring ListCtrl::get_selected() const
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
	const wstring selected_name = get_selected();  // [ListCtrl.md#selection-restored]
	DeleteAllItems();  // [ListCtrl.md#items-only]
	add_data_items();
	if (!selected_name.empty()) {
		for (long i = 0; i < GetItemCount(); ++i) {
			if (GetItemText(i).ToStdWstring() == selected_name) {
				SetItemState(i, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
				EnsureVisible(i);
				break;
			}
		}
	}
}

void ListCtrl::on_double_click(wxMouseEvent&)
{
	const wstring sync_name = get_selected();
	std::optional<SyncItem> found = nullopt;
	if (!sync_name.empty())
		found = Config::instance().find_by_name(sync_name);
	if (!found.has_value()) // double-click on empty list area, or item no longer exists
		return;
	const std::wstring original_name = found->name();  // [ListCtrl.md#copy-written-back]
	SyncItemDialog dlg(&*found, this, wxID_ANY, txt::edit_synchronisation());
	if (dlg.ShowModal() == wxID_OK) {
		dlg.save_data(*found);
		Config::instance().update_by_name(original_name, *found);
		autosave::after_change(this);  // [AutoSave.md#after_change]
		update();
	}
}