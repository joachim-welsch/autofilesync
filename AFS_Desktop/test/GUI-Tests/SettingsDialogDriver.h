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
