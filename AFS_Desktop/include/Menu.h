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
#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class Menu {
	wxMenu*		edit_menu_ = nullptr;
	wxMenu*		view_menu_ = nullptr;
	wxMenu*		sync_menu_ = nullptr;
	wxMenu*		background_service_menu_ = nullptr;
	wxMenuBar*	menu_bar_ = nullptr;

	wxMenu* create_file_menu();
	wxMenu* create_edit_menu();
	wxMenu* create_view_menu();
	wxMenu* create_sync_menu();
	wxMenu* create_background_service_menu();
	wxMenu* create_assistant_menu();
	wxMenu* create_help_menu();
public:
	void create();
	wxMenuBar* menu_bar();
	wxMenu* edit_menu();
	wxMenu* view_menu();
	wxMenu* sync_menu();
	wxMenu* background_service_menu();
	~Menu();
};

