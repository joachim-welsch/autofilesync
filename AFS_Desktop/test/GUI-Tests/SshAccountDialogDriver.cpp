#include "SshAccountDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SshAccountDialog.h"
#include "SshAccountsDialog.h"

namespace guitest {
namespace {

bool enterValues(wxDialog& dialog, const SshAccountInput& input)  // [SshAccountDialogDriver.md#enterValues]
{
    const struct { wxWindowID id; wxString text; const char* label; } fields[] = {
        { ID_SSH_NAME_BOX, input.name, "Name" },
        { ID_SSH_HOST_BOX, input.host, "Host" },
        { ID_SSH_USER_BOX, input.user, "Benutzer" },
        { ID_SSH_PASSWORD_BOX, input.password, "Passwort" },
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

DialogAction fillSshAccountDialog(const SshAccountInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK));
        if (!confirmed)  // [SshAccountDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

DialogAction addSshAccountAndClose()
{
    return [](wxDialog& dialog) {
        if (waitUntil([&dialog] { return dialog.IsShownOnScreen(); }))  // [SshAccountDialogDriver.md#nested-dialog-already-closed]
            clickOn(find(dialog, ID_SSHACCOUNTS_ADD));
        dialog.EndModal(wxID_OK);
    };
}

}
