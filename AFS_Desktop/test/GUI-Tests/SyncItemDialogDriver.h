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
