#include "LoggingDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "LoggingDialog.h"

namespace guitest {
namespace {

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
    if (input.logLevel && !selectEntry(find(dialog, ID_LOG_LEVEL_LISTBOX), *input.logLevel)) {
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
    return showNotebookPage(dialog, _("Logging")) && enterLoggingPage(dialog, input)
        && showNotebookPage(dialog, _("E-Mail")) && enterEmailPage(dialog, input.email)
        && showNotebookPage(dialog, _("Logging"));
}

}

DialogAction fillLoggingDialog(const LoggingInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK))
            && waitUntil([&dialog] { return !dialog.IsShown(); });  // [LoggingDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [LoggingDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
