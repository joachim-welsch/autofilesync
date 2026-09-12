#pragma once

#include "ModalDialogWatcher.h"
#include "Settings.h"
#include <wx/string.h>

using namespace AFS_Core;

namespace guitest {

DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode);  // [SettingsDialogDriver.md#fillSettingsDialog]

}
