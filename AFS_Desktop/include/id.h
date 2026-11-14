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

enum class id {  // [id.md#id]
	ToolBar = 500,
	ListCtrl,
	GridCtrl,
	Quit = wxID_EXIT,
	About = wxID_ABOUT,
	Help = wxID_HELP,
	EditSettings = wxID_PREFERENCES,  // [id.md#stock-ids]
	MAINWINDOW = wxID_HIGHEST + 1,
	SyncOnce,
	Preview,
	StartSync,
	StopSync,
	EditSync,
	EditSshAccounts,
	EditLogging,
	DeleteSync,
	File_new,
	File_new_SshConnection,
	File_new_Synchronization,
	File_open,
	File_save,
	File_save_as,
	View_Tree,
	View_List,
	View_Grid,
	View_Log,
    View_Background,
    BackgroundServiceState,
    BackgroundServiceStart,
    BackgroundServiceStop,
    Chat,
    File_connect_phone,
    EditDevices,
    RestoreFiles,
    File_new_WebDavConnection
};
