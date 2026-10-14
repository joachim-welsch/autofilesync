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

#include "SyncItemDialogDriver.h"
#include <optional>
#include <vector>

class MainWindow;
class ProgressDialog;
class wxGauge;
class wxString;

namespace guitest {  // [SyncActions.md#SyncActions]

constexpr int SYNC_TIMEOUT_MILLISECONDS = 30000;

bool add_sync_item(MainWindow& window, const SyncItemInput& input);  // [SyncActions.md#add_sync_item]
bool wait_until_sync_finished(MainWindow& window, int timeout_milliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#wait_until_sync_finished]
bool sync_once(MainWindow& window, int timeout_milliseconds = SYNC_TIMEOUT_MILLISECONDS);  // [SyncActions.md#sync_once]
bool sync_menu_item_enabled(MainWindow& window, int menu_item_id);
ProgressDialog* progress_dialog();  // [SyncActions.md#progress_dialog]
wxString current_file_shown(ProgressDialog& dialog);  // [SyncActions.md#current_file_shown]
wxGauge* gauge_of(ProgressDialog& dialog);

struct ProgressWatch {  // [SyncActions.md#watch_progress_dialog]
    std::vector<int> bar_values;
    std::vector<wxString> files;
};
std::optional<ProgressWatch> watch_progress_dialog(int timeout_milliseconds = SYNC_TIMEOUT_MILLISECONDS);

}
