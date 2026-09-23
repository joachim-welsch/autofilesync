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
#include "SyncItem.h"
#include <wx/string.h>
#include <optional>

using namespace AFS_Core;

namespace guitest {

struct SyncItemOptions {  // [SyncItemDialogDriver.md#SyncItemOptions]
    std::optional<bool> enabled;
    std::optional<bool> deleteSourceAfter;
    std::optional<bool> twoWaySync;
    std::optional<bool> copyWhenTargetNewer;
    std::optional<bool> fatherSonPrinciple;
    std::optional<bool> deleteInTarget;
    std::optional<bool> backupProof;
    std::optional<bool> syncEmptyFiles;
    std::optional<bool> compareDate;
    std::optional<bool> compareSize;
    std::optional<bool> compareContent;
    std::optional<int> syncCycleSeconds;
};

struct SyncItemInput {
    wxString name;
    wxString source;
    wxString target;
    std::optional<SyncItemType> type;
    std::optional<wxString> wildcard;
    std::optional<wxString> exclusions;
    SyncItemOptions options;
};

DialogAction fillSyncItemDialog(const SyncItemInput& input);  // [SyncItemDialogDriver.md#fillSyncItemDialog]

}
