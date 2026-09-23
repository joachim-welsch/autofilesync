/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include <stdint.h>
#include <string>

namespace AFS_Core {

enum class DeleteMode { RecycleBin, Permanent };

class Settings {
    DeleteMode delete_mode_          = DeleteMode::RecycleBin;  // [Settings.md#delete_mode_]
    bool console_mode_               = false;
    bool start_minimized_            = false;
    bool minimize_instead_of_close_    = false;
    bool auto_start_                 = false;
    bool start_sync_at_start_          = false;
    bool show_errors_                = true;  // [Settings.md#show_errors_]
    bool show_warnings_              = false;
    bool show_infos_                 = false;
    std::wstring backup_proof_path_;  // [Settings.md#backup_proof_path_]
public:
    void set_console_mode() { console_mode_=true; }
    bool console_mode() const { return console_mode_; }
    bool start_minimized() const { return start_minimized_; }
    void start_minimized(bool sm) { start_minimized_=sm; }
    bool minimize_instead_of_close() const { return minimize_instead_of_close_; }
    void minimize_instead_of_close(bool m) { minimize_instead_of_close_ = m; }
    bool auto_start() const { return auto_start_; }
    void auto_start(bool as) { auto_start_=as; }
    bool start_sync_at_start() const { return start_sync_at_start_; }
    void start_sync_at_start(bool as) { start_sync_at_start_=as; }
    bool show_errors() const { return show_errors_; }
    void show_errors(bool t) { show_errors_ = t; }
    bool show_warnings() const { return show_warnings_; }
    void show_warnings(bool t) { show_warnings_ = t; }
    bool show_infos() const { return show_infos_; }
    void show_infos(bool t) { show_infos_ = t; }
    DeleteMode delete_mode() const { return delete_mode_; }
    void delete_mode(DeleteMode dm) { delete_mode_ = dm; }
    std::wstring backup_proof_path() const { return backup_proof_path_; }
    void backup_proof_path(const std::wstring& path) { backup_proof_path_ = path; }
    void reset();
};

} // namespace AFS_Core
