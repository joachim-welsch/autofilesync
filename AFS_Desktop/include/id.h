/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once

enum class id {  // [id.md#id]
	ToolBar = 500,
	ListCtrl,
	GridCtrl,
	Quit = wxID_EXIT,
	About = wxID_ABOUT,
	Help = wxID_HELP,
	MAINWINDOW = wxID_HIGHEST + 1,
	SyncOnce,
	StartSync,
	StopSync,
	EditSync,
	EditSshAccounts,
	EditSettings,
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
    Chat
};
