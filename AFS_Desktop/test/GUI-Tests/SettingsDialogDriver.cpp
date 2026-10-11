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
#include "SettingsDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SettingsDialog.h"
#include "Translation.h"

using namespace AFS_Core;

namespace guitest {
namespace {

bool enterCheckBox(wxDialog& dialog, const wxString& label, const std::optional<bool>& wanted)  // [SettingsDialogDriver.md#checkboxes-by-label]
{
    if (!wanted || setCheckBox(wxWindow::FindWindowByLabel(label, &dialog), *wanted))
        return true;
    ADD_FAILURE() << "\"" << label << "\" konnte nicht gesetzt werden.";
    return false;
}

bool enterValues(wxDialog& dialog, const SettingsInput& input)  // [SettingsDialogDriver.md#enterValues]
{
    if (input.deleteMode && !selectRadioOption(find(dialog, ID_DELETE_MODE_RADIO), *input.deleteMode == DeleteMode::Permanent ? 1 : 0)) {
        ADD_FAILURE() << "Der Löschmodus konnte im Dialog nicht ausgewählt werden.";
        return false;
    }
    if (input.language && !selectEntry(find(dialog, ID_LANGUAGE_CHOICE), *input.language)) {
        ADD_FAILURE() << "Die Sprache \"" << *input.language << "\" steht nicht in der Auswahl.";
        return false;
    }
    if (input.recycleBinPath && !replaceText(find(dialog, ID_RECYCLEBIN_PATH_BOX), *input.recycleBinPath)) {
        ADD_FAILURE() << "Der Papierkorb-Pfad konnte nicht eingegeben werden; im Feld steht: "
                      << valueOf(find(dialog, ID_RECYCLEBIN_PATH_BOX));
        return false;
    }
    return enterCheckBox(dialog, translation::translate("settings_autostart"), input.autostart)
        && enterCheckBox(dialog, translation::translate("settings_sync_at_start"), input.startSyncAtStart)
        && enterCheckBox(dialog, translation::translate("settings_start_minimized"), input.startMinimized)
        && enterCheckBox(dialog, translation::translate("settings_minimize_instead_of_close"), input.minimizeInsteadOfClose)
        && enterCheckBox(dialog, translation::translate("error"), input.showErrors)
        && enterCheckBox(dialog, translation::translate("warning"), input.showWarnings)
        && enterCheckBox(dialog, translation::translate("info"), input.showInfos);
}

}

DialogAction fillSettingsDialog(const SettingsInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && enterValues(dialog, input)
            && clickWhenEnabled(find(dialog, wxID_OK));
        if (!confirmed)  // [SettingsDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

DialogAction fillSettingsDialog(const wxString& recycleBinPath, DeleteMode deleteMode)
{
    SettingsInput input;
    input.recycleBinPath = recycleBinPath;
    input.deleteMode = deleteMode;
    return fillSettingsDialog(input);
}

}
