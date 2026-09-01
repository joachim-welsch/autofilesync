#include "SyncItemDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SyncItemDialog.h"

namespace guitest {
namespace {

// Type and wildcard keep the values the dialog starts with ("Datei", "*.*"):
// they are already valid, and neither is reachable with plain key strokes.
// Each field reports itself: without that, a failed interaction is
// indistinguishable from a broken feature - both end as "not created".
bool enterValues(wxDialog& dialog, const SyncItemInput& input)
{
    const struct { wxWindowID id; wxString text; const char* label; } fields[] = {
        { ID_NAME_BOX, input.name, "Name" },
        { ID_SOURCE_DIR_BOX, input.source, "Quelle" },
        { ID_TARGET_DIR_BOX, input.target, "Ziel" },
    };
    for (const auto& field : fields) {
        if (!typeInto(find(dialog, field.id), field.text)) {
            ADD_FAILURE() << field.label << " konnte nicht eingegeben werden.\n  erwartet: "
                          << field.text << "\n  im Feld:  " << valueOf(find(dialog, field.id));
            return false;
        }
    }
    return true;
}

}

DialogAction fillSyncItemDialog(const SyncItemInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK));
        // Leaving the dialog open would hang the test in its modal loop; the
        // cancel result tells the test that the interaction, not the feature,
        // is what failed.
        if (!confirmed)
            dialog.EndModal(wxID_CANCEL);
    };
}

}
