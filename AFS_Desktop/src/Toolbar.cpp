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
#include "Toolbar.h"
#include "id.h"
#include "utilwx.h"
#include "Translation.h"
#ifdef _UNIX
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wwrite-strings"

    #include "../bitmaps/new.xpm"  // [Toolbar.md#xpm-icons]
    #include "../bitmaps/edit.xpm"
    #include "../bitmaps/save.xpm"
    #include "../bitmaps/delete.xpm"
    #include "../bitmaps/start.xpm"
    #include "../bitmaps/stop.xpm"
    #include "../bitmaps/help.xpm"
    #include "../bitmaps/settings.xpm"

    #pragma GCC diagnostic pop
#endif

using utilwx::tul;

namespace {
wxString tooltip(const char* msgid)  // [Toolbar.md#tooltips-without-ellipsis]
{
    wxString label = AFS_Core::translation::translate(msgid);
    if (label.EndsWith(L"\u2026"))
        label.RemoveLast();
    return label;
}
}

wxToolBar* Toolbar::toolbar()
{
	return toolbar_;
}

void Toolbar::create(wxFrame* parent)
{
	toolbar_ = parent->CreateToolBar(wxTB_FLAT | wxTB_NODIVIDER | wxTB_HORIZONTAL, tul(id::ToolBar));
	toolbar_->SetToolBitmapSize(wxSize(24, 24));
	toolbar_->AddTool(tul(id::File_new_Synchronization), wxEmptyString, wxBITMAP(new), tooltip("menu_new_synchronisation"));  // [Toolbar.md#new-first]
	toolbar_->AddTool(tul(id::EditSync), wxEmptyString, wxBITMAP(edit), tooltip("edit_synchronisation"));
	toolbar_->AddTool(tul(id::File_save), wxEmptyString, wxBITMAP(save), tooltip("toolbar_save"));
	toolbar_->AddTool(tul(id::DeleteSync), wxEmptyString, wxBITMAP(delete), tooltip("delete_synchronisation"));
	toolbar_->AddTool(tul(id::StartSync), wxEmptyString, wxBITMAP(start), tooltip("toolbar_start_sync"));

	toolbar_->AddStretchableSpace();
	toolbar_->AddTool(tul(id::Help), wxEmptyString, wxBITMAP(help), tooltip("toolbar_help"));
	toolbar_->AddTool(tul(id::EditSettings), wxEmptyString, wxBITMAP(settings), tooltip("menu_settings"));
	toolbar_->Realize();
}


void Toolbar::replace_tool(int old_tool_id, int new_tool_id, const wxString& label, const wxBitmap& new_bitmap, const wxString& short_help)
{
    size_t pos = toolbar_->GetToolPos(old_tool_id);
    if (pos != wxNOT_FOUND) {
        toolbar_->DeleteTool(old_tool_id);
        toolbar_->InsertTool(pos, new_tool_id, label, new_bitmap, wxBitmapBundle(), wxITEM_NORMAL, short_help);
        toolbar_->Realize();
    }
}

void Toolbar::set_started(bool started)
{
    if (started) {
        replace_tool(tul(id::StartSync), tul(id::StopSync), tooltip("toolbar_stop_sync"), wxBITMAP(stop), tooltip("toolbar_stop_sync"));
    }
    else {
        replace_tool(tul(id::StopSync), tul(id::StartSync), tooltip("toolbar_start_sync"), wxBITMAP(start), tooltip("toolbar_start_sync"));
    }
}
