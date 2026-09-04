#pragma once

#include "ModalDialogWatcher.h"
#include "Settings.h"
#include <wx/string.h>

namespace guitest {

// Enters a recycle bin path and a delete mode in the "Globale Einstellungen"
// dialog the way a user would and confirms it with OK. Pass the result to
// ModalDialogWatcher::expect().
DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode);

}
