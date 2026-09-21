#include "SettingsDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SettingsDialog.h"

using namespace AFS_Core;

namespace guitest {
namespace {

bool enterCheckBox(wxDialog& dialog, const wxString& label, const std::optional<bool>& wanted)  // [SettingsDialogDriver.md#checkboxes-by-label]
{
    if (!wanted || setCheckBox(wxWindow::FindWindowByLabel(label, &dialog), *wanted))
        return true;
    ADD_FAILURE() << "\"" << label << "\" konnte nicht gesetzt werden.";
    return false;
}

bool enterValues(wxDialog& dialog, const SettingsInput& input)  // [SettingsDialogDriver.md#enterValues]
{
    if (input.deleteMode && !selectRadioOption(find(dialog, ID_DELETE_MODE_RADIO), *input.deleteMode == DeleteMode::Permanent ? 1 : 0)) {
        ADD_FAILURE() << "Der Löschmodus konnte im Dialog nicht ausgewählt werden.";
        return false;
    }
    if (input.recycleBinPath && !replaceText(find(dialog, ID_RECYCLEBIN_PATH_BOX), *input.recycleBinPath)) {
        ADD_FAILURE() << "Der Papierkorb-Pfad konnte nicht eingegeben werden; im Feld steht: "
                      << valueOf(find(dialog, ID_RECYCLEBIN_PATH_BOX));
        return false;
    }
    return enterCheckBox(dialog, "Anwendung automatisch starten", input.autostart)
        && enterCheckBox(dialog, "Synchronisation bei Programmstart starten", input.startSyncAtStart)
        && enterCheckBox(dialog, "Start minimiert", input.startMinimized)
        && enterCheckBox(dialog, L"Minimieren statt schließen?", input.minimizeInsteadOfClose)
        && enterCheckBox(dialog, "Fehler", input.showErrors)
        && enterCheckBox(dialog, "Warnung", input.showWarnings)
        && enterCheckBox(dialog, "Info", input.showInfos);
}

}

DialogAction fillSettingsDialog(const SettingsInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK));
        if (!confirmed)  // [SettingsDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode)
{
    SettingsInput input;
    input.recycleBinPath = recycleBinPath;
    input.deleteMode = deleteMode;
    return fillSettingsDialog(input);
}

}
