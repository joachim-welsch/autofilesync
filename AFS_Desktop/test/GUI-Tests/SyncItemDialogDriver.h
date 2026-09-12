#pragma once

#include "ModalDialogWatcher.h"
#include <wx/string.h>

namespace guitest {

struct SyncItemInput {
    wxString name;
    wxString source;
    wxString target;
};

DialogAction fillSyncItemDialog(const SyncItemInput& input);  // [SyncItemDialogDriver.md#fillSyncItemDialog]

}
