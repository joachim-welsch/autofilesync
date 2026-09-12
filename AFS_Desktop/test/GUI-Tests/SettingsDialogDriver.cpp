#include "SettingsDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SettingsDialog.h"

using namespace AFS_Core;

namespace guitest {
namespace {

bool enterValues(wxDialog& dialog, const wxString& recycleBinPath, DeleteMode deleteMode)  // [SettingsDialogDriver.md#enterValues]
{
    if (!selectRadioOption(find(dialog, ID_DELETE_MODE_RADIO), deleteMode == DeleteMode::Permanent ? 1 : 0)) {
        ADD_FAILURE() << "Der Löschmodus konnte im Dialog nicht ausgewählt werden.";
        return false;
    }
    if (!replaceText(find(dialog, ID_RECYCLEBIN_PATH_BOX), recycleBinPath)) {
        ADD_FAILURE() << "Der Papierkorb-Pfad konnte nicht eingegeben werden; im Feld steht: "
                      << valueOf(find(dialog, ID_RECYCLEBIN_PATH_BOX));
        return false;
    }
    return true;
}

}

DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode)
{
    return [recycleBinPath, deleteMode](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, recycleBinPath, deleteMode)
            && clickWhenEnabled(find(dialog, wxID_OK));
        if (!confirmed)  // [SettingsDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
