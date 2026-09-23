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
#include "SyncItemDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SyncItemDialog.h"
#include "Translation.h"

namespace guitest {
namespace {

constexpr int CLOSE_TIMEOUT_MILLISECONDS = 2000;  // a dialog that OK does not close is being kept open by a message box, not by slowness

bool enterText(wxDialog& dialog, wxWindowID id, const wxString& text, const char* label)
{
    if (replaceText(find(dialog, id), text))
        return true;
    ADD_FAILURE() << label << " konnte nicht eingegeben werden.\n  erwartet: " << text
                  << "\n  im Feld:  " << valueOf(find(dialog, id));
    return false;
}

bool enterLines(wxDialog& dialog, wxWindowID id, const wxString& lines, const char* label)
{
    if (replaceLines(find(dialog, id), lines))
        return true;
    ADD_FAILURE() << label << " konnten nicht eingegeben werden; im Feld steht: " << valueOf(find(dialog, id));
    return false;
}

bool enterGeneral(wxDialog& dialog, const SyncItemInput& input)  // [SyncItemDialogDriver.md#enterValues]
{
    if (!enterText(dialog, ID_NAME_BOX, input.name, "Name"))
        return false;
    if (input.type && !selectEntry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(*input.type))) {
        ADD_FAILURE() << "Der Typ konnte nicht ausgewählt werden.";
        return false;
    }
    if (input.wildcard && !enterText(dialog, ID_WILDCARD_BOX, *input.wildcard, "Platzhalter"))
        return false;
    return enterText(dialog, ID_SOURCE_DIR_BOX, input.source, "Quelle")
        && enterText(dialog, ID_TARGET_DIR_BOX, input.target, "Ziel");
}

bool enterOptions(wxDialog& dialog, const SyncItemOptions& options)  // [SyncItemDialogDriver.md#enterOptions]
{
    const struct { std::optional<bool> wanted; wxWindowID id; const char* label; } boxes[] = {
        { options.enabled, ID_ENABLED_CHECK, "Aktiv" },
        { options.compareDate, ID_COMPARE_DATE_CHECK, "Vergleich nach Datum" },
        { options.compareSize, ID_COMPARE_SIZE_CHECK, "Vergleich nach Größe" },
        { options.compareContent, ID_COMPARE_CONTENT_CHECK, "Vergleich nach Inhalt" },
        { options.deleteSourceAfter, ID_DEL_SOURCE_AFTER_CHECK, "Quelle nach Kopieren löschen" },
        { options.twoWaySync, ID_TWO_WAY_SYNC_CHECK, "Zwei-Wege-Sync" },
        { options.copyWhenTargetNewer, ID_CPY_WHEN_TARGET_NEWER_CHECK, "Kopieren wenn Ziel neuer" },
        { options.fatherSonPrinciple, ID_FATHER_SON_PRINCIPLE_CHECK, "Generationenprinzip" },
        { options.deleteInTarget, ID_DELETE_IN_TARGET_CHECK, "Gelöschte Dateien im Ziel löschen" },
        { options.backupProof, ID_BACKUP_PROOF_CHECK, "Backup-Nachweis" },
        { options.syncEmptyFiles, ID_SYNC_EMPTY_FILES_CHECK, "Leere Dateien synchronisieren" },
    };
    for (const auto& box : boxes) {
        if (box.wanted && !setCheckBox(find(dialog, box.id), *box.wanted)) {
            ADD_FAILURE() << "Die Option \"" << box.label << "\" konnte nicht gesetzt werden.";
            return false;
        }
    }
    if (options.syncCycleSeconds && !setSpinValue(find(dialog, ID_SYNC_CYCLE_SPIN), *options.syncCycleSeconds)) {
        ADD_FAILURE() << "Der Zyklus konnte nicht auf " << *options.syncCycleSeconds << " gesetzt werden.";
        return false;
    }
    return true;
}

bool enterValues(wxDialog& dialog, const SyncItemInput& input)
{
    if (!showNotebookPage(dialog, AFS_Core::translation::translate("tab_general")) || !enterGeneral(dialog, input))
        return false;
    if (!showNotebookPage(dialog, AFS_Core::translation::translate("tab_options")) || !enterOptions(dialog, input.options))
        return false;
    if (input.exclusions && (!showNotebookPage(dialog, AFS_Core::translation::translate("tab_exclusions"))
            || !enterLines(dialog, ID_EXCLUSIONS_BOX, *input.exclusions, "Die Ausschlüsse")))
        return false;
    return showNotebookPage(dialog, AFS_Core::translation::translate("tab_general"));
}

}

DialogAction fillSyncItemDialog(const SyncItemInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK))
            && waitUntil([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [SyncItemDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [SyncItemDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
