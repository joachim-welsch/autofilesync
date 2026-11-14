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
#include "Translation.h"
#include <wx/string.h>

namespace txt {  // [TextResources.md#txt]
inline wxString add_synchronisation() { return AFS_Core::translation::translate("title_add_synchronisation"); }
inline wxString edit_synchronisation() { return AFS_Core::translation::translate("title_edit_synchronisation"); }
inline wxString add_ssh_connection() { return AFS_Core::translation::translate("title_add_ssh_connection"); }
inline wxString edit_ssh_connection() { return AFS_Core::translation::translate("title_edit_ssh_connection"); }
inline wxString ssh_connections() { return AFS_Core::translation::translate("title_ssh_connections"); }
inline wxString select_ssh_connection() { return AFS_Core::translation::translate("title_select_ssh_connection"); }
inline wxString select_ssh_dir() { return AFS_Core::translation::translate("title_select_ssh_dir"); }
inline wxString connect_phone() { return AFS_Core::translation::translate("title_connect_phone"); }
inline wxString devices() { return AFS_Core::translation::translate("title_devices"); }
inline wxString phone_dir() { return AFS_Core::translation::translate("title_phone_dir"); }
inline wxString cloud_folder() { return AFS_Core::translation::translate("title_cloud_folder"); }
inline wxString phone_templates() { return AFS_Core::translation::translate("title_phone_templates"); }
inline wxString settings() { return AFS_Core::translation::translate("title_settings"); }
inline wxString logging_settings() { return AFS_Core::translation::translate("title_logging_settings"); }
inline wxString assistant() { return AFS_Core::translation::translate("title_assistant"); }
}
