#include "SshAccountDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SshAccountDialog.h"
#include "SshAccountsDialog.h"
#include <wx/listctrl.h>
#include <wx/uiaction.h>

namespace guitest {
namespace {

constexpr int CLOSE_TIMEOUT_MILLISECONDS = 2000;  // a dialog that OK does not close is being kept open by a message box, not by slowness

bool enterValues(wxDialog& dialog, const SshAccountInput& input)  // [SshAccountDialogDriver.md#enterValues]
{
    const struct { wxWindowID id; wxString text; const char* label; } fields[] = {
        { ID_SSH_NAME_BOX, input.name, "Name" },
        { ID_SSH_HOST_BOX, input.host, "Host" },
        { ID_SSH_USER_BOX, input.user, "Benutzer" },
        { ID_SSH_PASSWORD_BOX, input.password, "Passwort" },
    };
    for (const auto& field : fields) {
        if (!replaceText(find(dialog, field.id), field.text)) {
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
            && clickWhenEnabled(find(dialog, wxID_OK))
            && waitUntil([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [SshAccountDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [SshAccountDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

namespace {

long rowOf(wxDialog& dialog, const wxString& accountName)
{
    auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
    if (list == nullptr)
        return -1;
    for (long row = 0; row < list->GetItemCount(); ++row)
        if (list->GetItemText(row) == accountName)
            return row;
    return -1;
}

bool selectRow(wxDialog& dialog, const wxString& accountName)
{
    auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
    const long row = rowOf(dialog, accountName);
    if (row < 0 || !clickOn(*list, row)) {
        ADD_FAILURE() << "Die SSH-Verbindung \"" << accountName << "\" steht nicht in der Liste oder ließ sich nicht auswählen.";
        return false;
    }
    return true;
}

}

DialogAction editSshAccountAndClose(const wxString& accountName)
{
    return [accountName](wxDialog& dialog) {
        if (waitUntil([&dialog] { return dialog.IsShownOnScreen(); }) && selectRow(dialog, accountName))
            clickWhenEnabled(find(dialog, ID_SSHACCOUNTS_EDIT));  // [SshAccountDialogDriver.md#nested-dialog-already-closed]
        dialog.EndModal(wxID_OK);
    };
}

DialogAction deleteSshAccountAndClose(const wxString& accountName)
{
    return [accountName](wxDialog& dialog) {
        if (waitUntil([&dialog] { return dialog.IsShownOnScreen(); }) && selectRow(dialog, accountName))
            clickWhenEnabled(find(dialog, ID_SSHACCOUNTS_DELETE));
        dialog.EndModal(wxID_OK);
    };
}

DialogAction renameSshAccountAndClose(const wxString& accountName, const wxString& newName)  // [SshAccountDialogDriver.md#renameSshAccountAndClose]
{
    return [accountName, newName](wxDialog& dialog) {
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
        if (waitUntil([&dialog] { return dialog.IsShownOnScreen(); }) && selectRow(dialog, accountName)) {
            wxTextCtrl* editor = list->EditLabel(rowOf(dialog, accountName));
            pump();
            wxUIActionSimulator simulator;
            if (editor == nullptr || !replaceText(editor, newName) || !simulator.Char(WXK_RETURN))
                ADD_FAILURE() << "Der Name ließ sich nicht in der Liste bearbeiten.";
            pump();
        }
        dialog.EndModal(wxID_OK);
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
