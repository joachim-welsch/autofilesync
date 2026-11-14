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
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <atomic>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>
#include "Restore.h"

class RestoreList;

class RestoreDialog : public wxDialog {  // [RestoreDialog.md#RestoreDialog]
public:
    RestoreDialog(wxWindow* parent, wxWindowID id, const wxString& title);
    ~RestoreDialog() override;
    wxString cell_text(long row, long column) const;
private:
    void start_search();
    void show_found(AFS_Core::Restorables found);
    void apply_filter();
    bool matches_filter(const AFS_Core::Restorable& file, const wxString& filter) const;
    std::vector<std::size_t> selected() const;
    std::optional<std::filesystem::path> ask_directory();
    void put_back(bool ask_where);
    void report(const std::vector<std::filesystem::path>& restored, const AFS_Core::Restorable& last, std::size_t failed);
    void forget_restored(const std::vector<std::size_t>& restored);
    void on_put_back(wxCommandEvent& event);
    void on_put_elsewhere(wxCommandEvent& event);
    void on_update_restore_button(wxUpdateUIEvent& event);

    AFS_Core::Restorables found_;
    std::vector<std::size_t> shown_;  // [RestoreDialog.md#filter]
    bool searching_ = true;
    std::atomic<bool> stop_search_{false};
    std::thread search_;  // [RestoreDialog.md#search-thread]
    wxSearchCtrl* filter_box_ = nullptr;
    RestoreList* list_ = nullptr;
    wxStaticText* status_ = nullptr;
};

enum {
    ID_RESTORE_LIST = wxID_HIGHEST + 330,
    ID_RESTORE_FILTER,
    ID_RESTORE_PUT_BACK,
    ID_RESTORE_PUT_ELSEWHERE,
    ID_RESTORE_STATUS
};

enum RestoreColumn { RESTORE_COLUMN_FILE, RESTORE_COLUMN_FOLDER, RESTORE_COLUMN_KIND, RESTORE_COLUMN_TIME, RESTORE_COLUMN_SIZE };
