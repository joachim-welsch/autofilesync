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
#include "LoggingDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "LoggingDialog.h"
#include "Translation.h"

namespace guitest {
namespace {

constexpr int CLOSE_TIMEOUT_MILLISECONDS = 2000;  // a dialog that OK does not close is being kept open by a message box, not by slowness

bool enter_text(wxDialog& dialog, wxWindowID id, const std::optional<wxString>& text, const char* label)
{
    if (!text || replace_text(find(dialog, id), *text))
        return true;
    ADD_FAILURE() << label << " konnte nicht eingegeben werden.\n  erwartet: " << *text
                  << "\n  im Feld:  " << value_of(find(dialog, id));
    return false;
}

bool enter_number(wxDialog& dialog, wxWindowID id, const std::optional<int>& value, const char* label)
{
    if (!value || set_spin_value(find(dialog, id), *value))
        return true;
    ADD_FAILURE() << label << " konnte nicht auf " << *value << " gesetzt werden.";
    return false;
}

bool enter_logging_page(wxDialog& dialog, const LoggingInput& input)  // [LoggingDialogDriver.md#enter_values]
{
    if (input.log_level && !select_entry(find(dialog, ID_LOG_LEVEL_CHOICE), *input.log_level)) {
        ADD_FAILURE() << "Der Log Level \"" << *input.log_level << "\" konnte nicht ausgewählt werden.";
        return false;
    }
    return enter_text(dialog, ID_LOG_PATH_BOX, input.log_path, "Der Protokolldatei-Pfad")
        && enter_number(dialog, ID_RETENTIONDAYS_SPIN, input.retention_days, "Die Historie")
        && enter_text(dialog, ID_EDIT_PATH_BOX, input.editor, "Der Editor")
        && enter_text(dialog, ID_BACKUP_PROOF_PATH_BOX, input.backup_proof_path, "Der Pfad für Sicherungsnachweise");
}

bool enter_email_page(wxDialog& dialog, const EmailInput& email)  // [LoggingDialogDriver.md#email-active-first]
{
    if (email.active && !set_check_box(find(dialog, ID_EMAIL_ACTIVE_CHECK), *email.active)) {
        ADD_FAILURE() << "\"Aktiv\" konnte nicht gesetzt werden.";
        return false;
    }
    return enter_text(dialog, ID_EMAIL_BOX, email.address, "Die E-Mail-Adresse")
        && enter_text(dialog, ID_SMTP_SERVER_BOX, email.server, "Der Postausgangsserver")
        && enter_number(dialog, ID_SMTP_PORT_SPIN, email.port, "Der Port")
        && enter_text(dialog, ID_SMTP_USER_BOX, email.user, "Der Benutzer")
        && enter_text(dialog, ID_SMTP_PASSWORD_BOX, email.password, "Das Passwort")
        && enter_text(dialog, ID_SMTP_FROM_BOX, email.sender, "Der Absender");
}

bool enter_values(wxDialog& dialog, const LoggingInput& input)
{
    return show_notebook_page(dialog, AFS_Core::translation::translate("tab_logging")) && enter_logging_page(dialog, input)
        && show_notebook_page(dialog, AFS_Core::translation::translate("tab_email")) && enter_email_page(dialog, input.email)
        && show_notebook_page(dialog, AFS_Core::translation::translate("tab_logging"));
}

}

DialogAction fill_logging_dialog(const LoggingInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
            && enter_values(dialog, input)
            && click_when_enabled(find(dialog, wxID_OK))
            && wait_until([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [LoggingDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [LoggingDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
