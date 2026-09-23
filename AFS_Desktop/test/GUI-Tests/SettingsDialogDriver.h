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

#include "ModalDialogWatcher.h"
#include "Settings.h"
#include <wx/string.h>
#include <optional>

using namespace AFS_Core;

namespace guitest {

struct SettingsInput {  // [SettingsDialogDriver.md#SettingsInput]
    std::optional<wxString> recycleBinPath;
    std::optional<DeleteMode> deleteMode;
    std::optional<bool> autostart;
    std::optional<bool> startSyncAtStart;
    std::optional<bool> startMinimized;
    std::optional<bool> minimizeInsteadOfClose;
    std::optional<bool> showErrors;
    std::optional<bool> showWarnings;
    std::optional<bool> showInfos;
};

DialogAction fillSettingsDialog(const SettingsInput& input);  // [SettingsDialogDriver.md#fillSettingsDialog]
DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode);

}
