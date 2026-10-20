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

bool enter_text(wxDialog& dialog, wxWindowID id, const wxString& text, const char* label)
{
    if (replace_text(find(dialog, id), text))
        return true;
    ADD_FAILURE() << label << " konnte nicht eingegeben werden.\n  erwartet: " << text
                  << "\n  im Feld:  " << value_of(find(dialog, id));
    return false;
}

bool enter_lines(wxDialog& dialog, wxWindowID id, const wxString& lines, const char* label)
{
    if (replace_lines(find(dialog, id), lines))
        return true;
    ADD_FAILURE() << label << " konnten nicht eingegeben werden; im Feld steht: " << value_of(find(dialog, id));
    return false;
}

bool enter_general(wxDialog& dialog, const SyncItemInput& input)  // [SyncItemDialogDriver.md#enter_values]
{
    if (!enter_text(dialog, ID_NAME_BOX, input.name, "Name"))
        return false;
    if (input.type && !select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(*input.type))) {
        ADD_FAILURE() << "Der Typ konnte nicht ausgewählt werden.";
        return false;
    }
    if (input.wildcard && !enter_text(dialog, ID_WILDCARD_BOX, *input.wildcard, "Platzhalter"))
        return false;
    return enter_text(dialog, ID_SOURCE_DIR_BOX, input.source, "Quelle")
        && enter_text(dialog, ID_TARGET_DIR_BOX, input.target, "Ziel");
}

bool enter_options(wxDialog& dialog, const SyncItemOptions& options)  // [SyncItemDialogDriver.md#enter_options]
{
    const struct { std::optional<bool> wanted; wxWindowID id; const char* label; } boxes[] = {
        { options.enabled, ID_ENABLED_CHECK, "Aktiv" },
        { options.compare_date, ID_COMPARE_DATE_CHECK, "Vergleich nach Datum" },
        { options.compare_size, ID_COMPARE_SIZE_CHECK, "Vergleich nach Größe" },
        { options.compare_content, ID_COMPARE_CONTENT_CHECK, "Vergleich nach Inhalt" },
        { options.delete_source_after, ID_DEL_SOURCE_AFTER_CHECK, "Quelle nach Kopieren löschen" },
        { options.two_way_sync, ID_TWO_WAY_SYNC_CHECK, "Zwei-Wege-Sync" },
        { options.copy_when_target_newer, ID_CPY_WHEN_TARGET_NEWER_CHECK, "Kopieren wenn Ziel neuer" },
        { options.father_son_principle, ID_FATHER_SON_PRINCIPLE_CHECK, "Generationenprinzip" },
        { options.delete_in_target, ID_DELETE_IN_TARGET_CHECK, "Gelöschte Dateien im Ziel löschen" },
        { options.backup_proof, ID_BACKUP_PROOF_CHECK, "Backup-Nachweis" },
        { options.sync_empty_files, ID_SYNC_EMPTY_FILES_CHECK, "Leere Dateien synchronisieren" },
        { options.warn_if_unreachable, ID_WARN_IF_UNREACHABLE_CHECK, "Warnung bei fehlendem Laufwerk" },
        { options.track_moves, ID_TRACK_MOVES_CHECK, "Umbenennen und Verschieben erkennen" },
    };
    for (const auto& box : boxes) {
        if (box.wanted && !set_check_box(find(dialog, box.id), *box.wanted)) {
            ADD_FAILURE() << "Die Option \"" << box.label << "\" konnte nicht gesetzt werden.";
            return false;
        }
    }
    if (options.sync_cycle_seconds && !set_spin_value(find(dialog, ID_SYNC_CYCLE_SPIN), *options.sync_cycle_seconds)) {
        ADD_FAILURE() << "Der Zyklus konnte nicht auf " << *options.sync_cycle_seconds << " gesetzt werden.";
        return false;
    }
    if (options.time_tolerance_seconds && !set_spin_value(find(dialog, ID_TIME_TOLERANCE_SPIN), *options.time_tolerance_seconds)) {
        ADD_FAILURE() << "Die Zeittoleranz konnte nicht auf " << *options.time_tolerance_seconds << " gesetzt werden.";
        return false;
    }
    return true;
}

bool enter_values(wxDialog& dialog, const SyncItemInput& input)
{
    if (!show_notebook_page(dialog, AFS_Core::translation::translate("tab_general")) || !enter_general(dialog, input))
        return false;
    if (!show_notebook_page(dialog, AFS_Core::translation::translate("tab_options")) || !enter_options(dialog, input.options))
        return false;
    if (input.exclusions && (!show_notebook_page(dialog, AFS_Core::translation::translate("tab_exclusions"))
            || !enter_lines(dialog, ID_EXCLUSIONS_BOX, *input.exclusions, "Die Ausschlüsse")))
        return false;
    return show_notebook_page(dialog, AFS_Core::translation::translate("tab_general"));
}

}

DialogAction fill_sync_item_dialog(const SyncItemInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
            && enter_values(dialog, input)
            && click_when_enabled(find(dialog, wxID_OK))
            && wait_until([&dialog] { return !dialog.IsShown(); }, CLOSE_TIMEOUT_MILLISECONDS);  // [SyncItemDialogDriver.md#rejected-input-keeps-the-dialog-open]
        if (!confirmed)  // [SyncItemDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

}
