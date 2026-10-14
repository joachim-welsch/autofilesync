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

bool enter_check_box(wxDialog& dialog, const wxString& label, const std::optional<bool>& wanted)  // [SettingsDialogDriver.md#checkboxes-by-label]
{
    if (!wanted || set_check_box(wxWindow::FindWindowByLabel(label, &dialog), *wanted))
        return true;
    ADD_FAILURE() << "\"" << label << "\" konnte nicht gesetzt werden.";
    return false;
}

bool enter_values(wxDialog& dialog, const SettingsInput& input)  // [SettingsDialogDriver.md#enter_values]
{
    if (input.delete_mode && !select_radio_option(find(dialog, ID_DELETE_MODE_RADIO), *input.delete_mode == DeleteMode::Permanent ? 1 : 0)) {
        ADD_FAILURE() << "Der Löschmodus konnte im Dialog nicht ausgewählt werden.";
        return false;
    }
    if (input.language && !select_entry(find(dialog, ID_LANGUAGE_CHOICE), *input.language)) {
        ADD_FAILURE() << "Die Sprache \"" << *input.language << "\" steht nicht in der Auswahl.";
        return false;
    }
    if (input.recycle_bin_path && !replace_text(find(dialog, ID_RECYCLEBIN_PATH_BOX), *input.recycle_bin_path)) {
        ADD_FAILURE() << "Der Papierkorb-Pfad konnte nicht eingegeben werden; im Feld steht: "
                      << value_of(find(dialog, ID_RECYCLEBIN_PATH_BOX));
        return false;
    }
    return enter_check_box(dialog, translation::translate("settings_autostart"), input.autostart)
        && enter_check_box(dialog, translation::translate("settings_sync_at_start"), input.start_sync_at_start)
        && enter_check_box(dialog, translation::translate("settings_start_minimized"), input.start_minimized)
        && enter_check_box(dialog, translation::translate("settings_minimize_instead_of_close"), input.minimize_instead_of_close)
        && enter_check_box(dialog, translation::translate("error"), input.show_errors)
        && enter_check_box(dialog, translation::translate("warning"), input.show_warnings)
        && enter_check_box(dialog, translation::translate("info"), input.show_infos);
}

}

DialogAction fill_settings_dialog(const SettingsInput& input)
{
    return [input](wxDialog& dialog) {
        const bool confirmed = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
            && enter_values(dialog, input)
            && click_when_enabled(find(dialog, wxID_OK));
        if (!confirmed)  // [SettingsDialogDriver.md#cancel-instead-of-hanging]
            dialog.EndModal(wxID_CANCEL);
    };
}

DialogAction fill_settings_dialog(const wxString& recycle_bin_path, DeleteMode delete_mode)
{
    SettingsInput input;
    input.recycle_bin_path = recycle_bin_path;
    input.delete_mode = delete_mode;
    return fill_settings_dialog(input);
}

}
