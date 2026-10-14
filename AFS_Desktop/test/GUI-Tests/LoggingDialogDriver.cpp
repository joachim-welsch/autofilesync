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

bool enterText(wxDialog& dialog, wxWindowID id, const std::optional<wxString>& text, const char* label)
{
    if (!text || replaceText(find(dialog, id), *text))
        return true;
    ADD_FAILURE() << label << " konnte nicht eingegeben werden.\n  erwartet: " << *text
                  << "\n  im Feld:  " << valueOf(find(dialog, id));
    return false;
}

bool enterNumber(wxDialog& dialog, wxWindowID id, const std::optional<int>& value, const char* label)
{
    if (!value || setSpinValue(find(dialog, id), *value))
        return true;
    ADD_FAILURE() << label << " konnte nicht auf " << *value << " gesetzt werden.";
    return false;
}

bool enterLoggingPage(wxDialog& dialog, const LoggingInput& input)  // [LoggingDialogDriver.md#enterValues]
{
    if (input.logLevel && !selectEntry(find(dialog, ID_LOG_LEVEL_CHOICE), *input.logLevel)) {
        ADD_FAILURE() << "Der Log Level \"" << *input.logLevel << "\" konnte nicht ausgewählt werden.";
        return false;
    }
    return enterText(dialog, ID_LOG_PATH_BOX, input.logPath, "Der Protokolldatei-Pfad")
        && enterNumber(dialog, ID_RETENTIONDAYS_SPIN, input.retentionDays, "Die Historie")
        && enterText(dialog, ID_EDIT_PATH_BOX, input.editor, "Der Editor")
        && enterText(dialog, ID_BACKUP_PROOF_PATH_BOX, input.backupProofPath, "Der Pfad für Sicherungsnachweise");
}

bool enterEmailPage(wxDialog& dialog, const EmailInput& email)  // [LoggingDialogDriver.md#email-active-first]
{
    if (email.active && !setCheckBox(find(dialog, ID_EMAIL_ACTIVE_CHECK), *email.active)) {
        ADD_FAILURE() << "\"Aktiv\" konnte nicht gesetzt werden.";
        return false;
    }
    return enterText(dialog, ID_EMAIL_BOX, email.address, "Die E-Mail-Adresse")
        && enterText(dialog, ID_SMTP_SERVER_BOX, email.server, "Der Postausgangsserver")
        && enterNumber(dialog, ID_SMTP_PORT_SPIN, email.port, "Der Port")
        && enterText(dialog, ID_SMTP_USER_BOX, email.user, "Der Benutzer")
        && enterText(dialog, ID_SMTP_PASSWORD_BOX, email.password, "Das Passwort")
        && enterText(dialog, ID_SMTP_FROM_BOX, email.sender, "Der Absender");
}

bool enterValues(wxDialog& dialog, const LoggingInput& input)
{
    return showNotebookPage(dialog, AFS_Core::translation::translate("tab_logging")) && enterLoggingPage(dialog, input)
        && showNotebookPage(dialog, AFS_Core::translation::translate("tab_email")) && enterEmailPage(dialog, input.email)
        && showNotebookPage(dialog, AFS_Core::translation::translate("tab_logging"));
}

}

DialogAction fillLoggingDialog(const LoggingInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK))
            && waitUntil([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [LoggingDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [LoggingDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
