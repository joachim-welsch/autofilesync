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

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>
#include "Global.h"
#include "Logging.h"

namespace AFS_Core { class SyncControler; }
class ProgressDialog;
class Menu;

using namespace AFS_Core;

class SyncInterface {
    wxWindow&       parent_window_;
    Menu&           menu_;
    SyncControler&  sync_controler_;
    ProgressDialog* progress_dialog_ = nullptr;
    std::unique_ptr<wxBusyCursor> counting_cursor_;  // [SyncInterface.md#counting_cursor_]
    bool            console_mode_ = false;
    SyncType        act_sync_type_ = SyncType::None;  // [SyncInterface.md#state_text]
    std::size_t     count_files_ = 0;
    std::chrono::high_resolution_clock::time_point start_time_;
    bool silent_mode() const;
    std::vector<LogMessage> take_run_messages();  // [SyncInterface.md#show_run_messages]
    void show_run_messages();
    void preview_finished();  // [SyncInterface.md#preview]
    bool start_pass(SyncType pass);
    void hide_progress_dialog();  // [SyncInterface.md#reuse-progress-dialog]
    wxFrame* parent_frame() { return dynamic_cast<wxFrame*>(&parent_window_); }
public:
    explicit SyncInterface(wxWindow& parent_window, Menu& menu);
    ~SyncInterface();
    void on_sync();
    void run_console_sync();
    bool start_sync();
    bool single_sync();
    bool preview();  // [SyncInterface.md#preview]
    void enable_start_commands(bool enabled);  // [SyncInterface.md#enable_start_commands]
    void set_console_mode();
    void open_progress_dialog(uint32_t n_files);
    void stop_sync();
    void sync_once_finished();
    void count_files_finished();
    void continuous_sync_finished();
    void on_timer();
    void start_continuous_sync();
    void stop_continuous_sync();
    wxString state_text() const;  // [SyncInterface.md#state_text]
    void show_state();  // [SyncInterface.md#show_state]
    SyncControler& sync_controler() { return sync_controler_; }
    SyncType act_sync_type() const { return act_sync_type_; }
    void set_act_sync_type(SyncType type) { act_sync_type_ = type; }
    bool is_continuously_syncing() const { return act_sync_type_ == SyncType::Continuous; }
};
