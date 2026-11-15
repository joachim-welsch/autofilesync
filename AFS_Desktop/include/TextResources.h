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
inline wxString add_synchronisation() { return Utilities::translation::translate("title_add_synchronisation"); }
inline wxString edit_synchronisation() { return Utilities::translation::translate("title_edit_synchronisation"); }
inline wxString add_ssh_connection() { return Utilities::translation::translate("title_add_ssh_connection"); }
inline wxString edit_ssh_connection() { return Utilities::translation::translate("title_edit_ssh_connection"); }
inline wxString ssh_connections() { return Utilities::translation::translate("title_ssh_connections"); }
inline wxString select_ssh_connection() { return Utilities::translation::translate("title_select_ssh_connection"); }
inline wxString select_ssh_dir() { return Utilities::translation::translate("title_select_ssh_dir"); }
inline wxString connect_phone() { return Utilities::translation::translate("title_connect_phone"); }
inline wxString devices() { return Utilities::translation::translate("title_devices"); }
inline wxString phone_dir() { return Utilities::translation::translate("title_phone_dir"); }
inline wxString cloud_folder() { return Utilities::translation::translate("title_cloud_folder"); }
inline wxString add_webdav_connection() { return Utilities::translation::translate("title_add_webdav_connection"); }
inline wxString edit_webdav_connection() { return Utilities::translation::translate("title_edit_webdav_connection"); }
inline wxString add_google_drive_connection() { return Utilities::translation::translate("title_add_google_drive_connection"); }
inline wxString edit_google_drive_connection() { return Utilities::translation::translate("title_edit_google_drive_connection"); }
inline wxString add_one_drive_connection() { return Utilities::translation::translate("title_add_one_drive_connection"); }
inline wxString edit_one_drive_connection() { return Utilities::translation::translate("title_edit_one_drive_connection"); }
inline wxString add_dropbox_connection() { return Utilities::translation::translate("title_add_dropbox_connection"); }
inline wxString edit_dropbox_connection() { return Utilities::translation::translate("title_edit_dropbox_connection"); }
inline wxString add_s3_connection() { return Utilities::translation::translate("title_add_s3_connection"); }
inline wxString edit_s3_connection() { return Utilities::translation::translate("title_edit_s3_connection"); }
inline wxString add_storage_box_connection() { return Utilities::translation::translate("title_add_storage_box_connection"); }
inline wxString restore_files() { return Utilities::translation::translate("title_restore_files"); }
inline wxString phone_templates() { return Utilities::translation::translate("title_phone_templates"); }
inline wxString settings() { return Utilities::translation::translate("title_settings"); }
inline wxString logging_settings() { return Utilities::translation::translate("title_logging_settings"); }
inline wxString assistant() { return Utilities::translation::translate("title_assistant"); }
}
