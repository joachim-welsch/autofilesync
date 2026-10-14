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
#include "SyncActions.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "Config.h"
#include "MainWindow.h"
#include "ProgressDialog.h"
#include "Synchronize.h"
#include "TextResources.h"
#include "id.h"
#include "utilwx.h"
#include <wx/gauge.h>
#include <wx/stattext.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

bool add_sync_item(MainWindow& window, const SyncItemInput& input)
{
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fill_sync_item_dialog(input));
    activate_menu_item(window, tul(id::File_new_Synchronization));
    pump();
    return Config::instance().find_by_name(input.name.ToStdWstring()).has_value();
}

bool wait_until_sync_finished(MainWindow& window, int timeout_milliseconds)
{
    return wait_until([&window] {  // [SyncActions.md#wait_until_sync_finished]
        return window.sync_interface().act_sync_type() == SyncType::None && !Synchronize::has_instance();
    }, timeout_milliseconds);
}

bool sync_once(MainWindow& window, int timeout_milliseconds)
{
    activate_menu_item(window, tul(id::SyncOnce));
    pump();
    return wait_until_sync_finished(window, timeout_milliseconds);
}

bool sync_menu_item_enabled(MainWindow& window, int menu_item_id)
{
    return window.menu().sync_menu()->IsEnabled(menu_item_id);
}

ProgressDialog* progress_dialog()
{
    for (wxWindow* window : wxTopLevelWindows) {
        if (auto* dialog = dynamic_cast<ProgressDialog*>(window); dialog != nullptr && dialog->IsShown())
            return dialog;
    }
    return nullptr;
}

wxString current_file_shown(ProgressDialog& dialog)
{
    for (wxWindow* child : dialog.GetChildren()) {
        if (auto* text = dynamic_cast<wxStaticText*>(child))
            return text->GetLabelText();
    }
    return wxString();
}

wxGauge* gauge_of(ProgressDialog& dialog)
{
    for (wxWindow* child : dialog.GetChildren()) {
        if (auto* gauge = dynamic_cast<wxGauge*>(child))
            return gauge;
    }
    return nullptr;
}

std::optional<ProgressWatch> watch_progress_dialog(int timeout_milliseconds)  // [SyncActions.md#watch_progress_dialog]
{
    ProgressWatch watch;
    const bool closed = wait_until([&watch] {
        ProgressDialog* dialog = progress_dialog();
        if (dialog == nullptr)
            return true;
        watch.bar_values.push_back(gauge_of(*dialog)->GetValue());
        const wxString file = current_file_shown(*dialog);
        if (!file.empty() && (watch.files.empty() || watch.files.back() != file))
            watch.files.push_back(file);
        return false;
    }, timeout_milliseconds);
    if (!closed)
        return std::nullopt;
    return watch;
}

}
