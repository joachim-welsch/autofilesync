/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "SshAccountDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SshAccountDialog.h"
#include "SshAccountsDialog.h"
#include <wx/listctrl.h>

namespace guitest {
namespace {

constexpr int CLOSE_TIMEOUT_MILLISECONDS = 2000;  // a dialog that OK does not close is being kept open by a message box, not by slowness

bool enter_values(wxDialog& dialog, const SshAccountInput& input)  // [SshAccountDialogDriver.md#enter_values]
{
    const struct { wxWindowID id; wxString text; const char* label; } fields[] = {
        { ID_SSH_NAME_BOX, input.name, "Name" },
        { ID_SSH_HOST_BOX, input.host, "Host" },
        { ID_SSH_USER_BOX, input.user, "Benutzer" },
        { ID_SSH_PASSWORD_BOX, input.password, "Passwort" },
    };
    for (const auto& field : fields) {
        if (!replace_text(find(dialog, field.id), field.text)) {
            ADD_FAILURE() << field.label << " konnte nicht eingegeben werden.\n  erwartet: "
                          << field.text << "\n  im Feld:  " << value_of(find(dialog, field.id));
            return false;
        }
    }
    return true;
}

}

DialogAction fill_ssh_account_dialog(const SshAccountInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
            && enter_values(dialog, input)
            && click_when_enabled(find(dialog, wxID_OK))
            && wait_until([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [SshAccountDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [SshAccountDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

namespace {

long row_of(wxDialog& dialog, const wxString& account_name)
{
    auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
    if (list == nullptr)
        return -1;
    for (long row = 0; row < list->GetItemCount(); ++row)
        if (list->GetItemText(row) == account_name)
            return row;
    return -1;
}

bool select_row(wxDialog& dialog, const wxString& account_name)
{
    auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
    const long row = row_of(dialog, account_name);
    if (row < 0 || !click_on(*list, row)) {
        ADD_FAILURE() << "Die SSH-Verbindung \"" << account_name << "\" steht nicht in der Liste oder ließ sich nicht auswählen.";
        return false;
    }
    return true;
}

}

DialogAction edit_ssh_account_and_close(const wxString& account_name)
{
    return [account_name](wxDialog& dialog) {
        if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }) && select_row(dialog, account_name))
            click_when_enabled(find(dialog, ID_SSHACCOUNTS_EDIT));  // [SshAccountDialogDriver.md#nested-dialog-already-closed]
        dialog.EndModal(wxID_OK);
    };
}

DialogAction delete_ssh_account_and_close(const wxString& account_name)
{
    return [account_name](wxDialog& dialog) {
        if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }) && select_row(dialog, account_name))
            click_when_enabled(find(dialog, ID_SSHACCOUNTS_DELETE));
        dialog.EndModal(wxID_OK);
    };
}

DialogAction rename_ssh_account_and_close(const wxString& account_name, const wxString& new_name)  // [SshAccountDialogDriver.md#rename_ssh_account_and_close]
{
    return [account_name, new_name](wxDialog& dialog) {
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSHACCOUNTS_LIST));
        if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }) && select_row(dialog, account_name)) {
            wxTextCtrl* editor = list->EditLabel(row_of(dialog, account_name));
            pump();
            if (editor == nullptr || !replace_text(editor, new_name) || !press_return())
                ADD_FAILURE() << "Der Name ließ sich nicht in der Liste bearbeiten.";
            pump();
        }
        dialog.EndModal(wxID_OK);
    };
}

DialogAction add_ssh_account_and_close()
{
    return [](wxDialog& dialog) {
        if (wait_until([&dialog] { return dialog.IsShownOnScreen(); }))  // [SshAccountDialogDriver.md#nested-dialog-already-closed]
            click_on(find(dialog, ID_SSHACCOUNTS_ADD));
        dialog.EndModal(wxID_OK);
    };
}

}
