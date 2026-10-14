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
#include "wx/spinctrl.h"
#endif
#include <wx/notebook.h>

#include "SyncItem.h"

using namespace AFS_Core;

class SyncItemDialog: public wxDialog {
public:
	SyncItemDialog(SyncItem* sync_item, 
        wxWindow* parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxASCII_STR(wxDialogNameStr));
    virtual ~SyncItemDialog();

    wxString name() const { return name_; }
    wxString type() const { return type_; }
    wxString wildcard() const { return wildcard_; }
    wxString source_dir() const { return source_; }
    wxString target_dir() const { return target_; }
    bool del_source_after() const { return del_source_after_; }
    bool two_way_sync() const { return two_way_sync_; }
    bool copy_when_target_newer() const { return copy_when_target_newer_; }
    bool father_son_principle() const { return father_son_principle_; }
    bool backup_proof() const { return backup_proof_; }
    bool sync_empty_files() const { return sync_empty_files_; }
    bool enabled() const { return enabled_; }
    void save_data(SyncItem& si);

private:
    void event_bindings();
    void update_delete_in_target_state();
    void update_time_tolerance_state();
    wxPanel* create_options_page(wxNotebook* notebook);

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    void on_update_ok_button(wxUpdateUIEvent& event);
    void on_pick_source_dir_text(wxCommandEvent& WXUNUSED(event));
    void on_pick_target_dir_text(wxCommandEvent& WXUNUSED(event));
    void on_ssh_source(wxCommandEvent& event);
    void on_ssh_target(wxCommandEvent& event);
    void handle_ssh_button(wxTextCtrl* target_box);
    void set_data(const SyncItem& si);
    SyncItemType act_sync_item_type(wxChoice* type_choice);
    
    wxString name_;
    wxString type_ = sync_item_type_to_str(SyncItemType::File); // must match an entry of the type choice, which is translated
    wxString wildcard_ = "*.*";
    wxString exclusions_; // one wildcard pattern per line, converted to/from Exclusions in save_data()/set_data()
    wxString source_;
    wxString target_;
    wxString volume_name_source_;
    wxString volume_name_target_;
    bool     enabled_ = true;
    bool     del_source_after_ = false;
    bool     two_way_sync_ = false;
    bool     copy_when_target_newer_ = false;
    bool     father_son_principle_ = false;
    bool     delete_in_target_ = false;
    bool     backup_proof_ = false;
    bool     sync_empty_files_ = false;
    bool     compare_date_ = true;
    bool     compare_size_ = false;
    bool     compare_content_ = false;
    uint16_t sync_cycle_ = 1;
    uint16_t time_tolerance_seconds_ = DEFAULT_TIME_TOLERANCE_SECONDS;
    
    wxTextCtrl* name_box_ = nullptr;
    wxChoice* type_choice_ = nullptr;
    wxTextCtrl* wildcard_box_ = nullptr;
    wxTextCtrl* exclusions_box_ = nullptr;
    wxTextCtrl* source_dir_box_ = nullptr;
    wxTextCtrl* target_dir_box_ = nullptr;
    wxCheckBox* enabled_check_ = nullptr;
    wxCheckBox* del_source_after_check_ = nullptr;
    wxCheckBox* compare_date_check_ = nullptr;
    wxCheckBox* two_way_sync_check_ = nullptr;
    wxCheckBox* compare_size_check_ = nullptr;
    wxCheckBox* copy_when_target_newer_check_ = nullptr;
    wxCheckBox* compare_content_check_ = nullptr;
    wxCheckBox* father_son_principle_check_ = nullptr;
    wxCheckBox* delete_in_target_check_ = nullptr;
    wxCheckBox* backup_proof_check_ = nullptr;
    wxCheckBox* sync_empty_files_check_ = nullptr;
    wxSpinCtrl* sync_cycle_spin_ = nullptr;
    wxSpinCtrl* time_tolerance_spin_ = nullptr;

    DECLARE_EVENT_TABLE()
};

enum {
    ID_NAME_BOX = wxID_HIGHEST + 1,
    ID_TYPE_CHOICE,
    ID_WILDCARD_BOX,
    ID_EXCLUSIONS_BOX,
    ID_SOURCE_DIR_BOX,
    ID_TARGET_DIR_BOX,
    ID_BTN_PICK_SOURCE_DIR,
    ID_BTN_PICK_TARGET_DIR,
    ID_BTN_SSH_SOURCE,
    ID_BTN_SSH_TARGET,
    ID_ENABLED_CHECK,
    ID_DEL_SOURCE_AFTER_CHECK,
    ID_COMPARE_DATE_CHECK,
    ID_TWO_WAY_SYNC_CHECK,
    ID_COMPARE_SIZE_CHECK,
    ID_CPY_WHEN_TARGET_NEWER_CHECK,
    ID_COMPARE_CONTENT_CHECK,
    ID_FATHER_SON_PRINCIPLE_CHECK,
    ID_DELETE_IN_TARGET_CHECK,
    ID_BACKUP_PROOF_CHECK,
    ID_SYNC_EMPTY_FILES_CHECK,
    ID_SYNC_CYCLE_SPIN,
    ID_TIME_TOLERANCE_SPIN
};
