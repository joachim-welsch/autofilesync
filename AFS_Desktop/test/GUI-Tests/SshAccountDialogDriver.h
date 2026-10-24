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
#pragma once

#include "ModalDialogWatcher.h"
#include <wx/string.h>

namespace guitest {

struct SshAccountInput {
    wxString name;
    wxString host;
    wxString user;
    wxString password;
    int port = 22;
};

DialogAction fill_ssh_account_dialog(const SshAccountInput& input);  // [SshAccountDialogDriver.md#fill_ssh_account_dialog]

DialogAction add_ssh_account_and_close();  // [SshAccountDialogDriver.md#add_ssh_account_and_close]
DialogAction edit_ssh_account_and_close(const wxString& account_name);  // [SshAccountDialogDriver.md#edit_ssh_account_and_close]
DialogAction delete_ssh_account_and_close(const wxString& account_name);  // [SshAccountDialogDriver.md#delete_ssh_account_and_close]
DialogAction rename_ssh_account_and_close(const wxString& account_name, const wxString& new_name);  // [SshAccountDialogDriver.md#rename_ssh_account_and_close]

}
