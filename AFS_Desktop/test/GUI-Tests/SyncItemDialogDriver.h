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
