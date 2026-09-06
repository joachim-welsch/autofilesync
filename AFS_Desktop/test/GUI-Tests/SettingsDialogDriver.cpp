#include "SettingsDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SettingsDialog.h"

using namespace AFS_Core;

namespace guitest {
namespace {

// The mode is set first: it decides whether the path field is enabled at all,
// and typing into a disabled field would silently do nothing. Each step reports
// itself: without that, a failed interaction is indistinguishable from a broken
// feature - both end as "the value was not taken over".
bool enterValues(wxDialog& dialog, const wxString& recycleBinPath, DeleteMode deleteMode)
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
        // Leaving the dialog open would hang the test in its modal loop; the
        // cancel result tells the test that the interaction, not the feature,
        // is what failed.
        if (!confirmed)
            dialog.EndModal(wxID_CANCEL);
    };
}

}
