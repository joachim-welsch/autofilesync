#pragma once

#include "ModalDialogWatcher.h"
#include <wx/string.h>

namespace guitest {

struct SyncItemInput {
    wxString name;
    wxString source;
    wxString target;
};

// Fills the "Allgemein" tab of a SyncItemDialog the way a user would and
// confirms it with OK. Pass the result to ModalDialogWatcher::expect().
DialogAction fillSyncItemDialog(const SyncItemInput& input);

}
