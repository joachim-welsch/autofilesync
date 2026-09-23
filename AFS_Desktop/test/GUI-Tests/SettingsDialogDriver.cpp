/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "SettingsDialogDriver.h"
#include <gtest/gtest.h>
#include "EventPump.h"
#include "UserActions.h"
#include "SettingsDialog.h"

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
    if (input.recycleBinPath && !replaceText(find(dialog, ID_RECYCLEBIN_PATH_BOX), *input.recycleBinPath)) {
        ADD_FAILURE() << "Der Papierkorb-Pfad konnte nicht eingegeben werden; im Feld steht: "
                      << valueOf(find(dialog, ID_RECYCLEBIN_PATH_BOX));
        return false;
    }
    return enterCheckBox(dialog, "Anwendung automatisch starten", input.autostart)
        && enterCheckBox(dialog, "Synchronisation bei Programmstart starten", input.startSyncAtStart)
        && enterCheckBox(dialog, "Start minimiert", input.startMinimized)
        && enterCheckBox(dialog, L"Minimieren statt schließen?", input.minimizeInsteadOfClose)
        && enterCheckBox(dialog, "Fehler", input.showErrors)
        && enterCheckBox(dialog, "Warnung", input.showWarnings)
        && enterCheckBox(dialog, "Info", input.showInfos);
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
