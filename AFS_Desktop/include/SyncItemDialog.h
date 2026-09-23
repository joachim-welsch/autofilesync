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
	SyncItemDialog(SyncItem* syncItem, 
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
    wxString sourceDir() const { return source_; }
    wxString targetDir() const { return target_; }
    bool delSourceAfter() const { return delSourceAfter_; }
    bool twoWaySync() const { return twoWaySync_; }
    bool copyWhenTargetNewer() const { return copyWhenTargetNewer_; }
    bool fatherSonPrinciple() const { return fatherSonPrinciple_; }
    bool backupProof() const { return backupProof_; }
    bool syncEmptyFiles() const { return syncEmptyFiles_; }
    bool enabled() const { return enabled_; }
    void saveData(SyncItem& si);

private:
    void eventBindings();
    void updateDeleteInTargetState();

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;
    bool Validate() override;

    void onUpdateOkButton(wxUpdateUIEvent& event);
    void onPickSourceDirText(wxCommandEvent& WXUNUSED(event));
    void onPickTargetDirText(wxCommandEvent& WXUNUSED(event));
    void onSshSource(wxCommandEvent& event);
    void onSshTarget(wxCommandEvent& event);
    void handleSshButton(wxTextCtrl* targetBox);
    void setData(const SyncItem& si);
    SyncItemType actSyncItemType(wxChoice* typeChoice);
    
    wxString name_;
    wxString type_ = sync_item_type_to_str(SyncItemType::File); // must match an entry of the type choice, which is translated
    wxString wildcard_ = "*.*";
    wxString exclusions_; // one wildcard pattern per line, converted to/from Exclusions in saveData()/setData()
    wxString source_;
    wxString target_;
    wxString volumneNameSource_;
    wxString volumneNameTarget_;
    bool     enabled_ = true;
    bool     delSourceAfter_ = false;
    bool     twoWaySync_ = false;
    bool     copyWhenTargetNewer_ = false;
    bool     fatherSonPrinciple_ = false;
    bool     deleteInTarget_ = false;
    bool     backupProof_ = false;
    bool     syncEmptyFiles_ = false;
    bool     compareDate_ = true;
    bool     compareSize_ = false;
    bool     compareContent_ = false;
    uint16_t syncCycle_ = 1;
    
    wxTextCtrl* nameBox_ = nullptr;
    wxChoice* typeChoice_ = nullptr;
    wxTextCtrl* wildcardBox_ = nullptr;
    wxTextCtrl* exclusionsBox_ = nullptr;
    wxTextCtrl* sourceDirBox_ = nullptr;
    wxTextCtrl* targetDirBox_ = nullptr;
    wxCheckBox* enabledCheck_ = nullptr;
    wxCheckBox* delSourceAfterCheck_ = nullptr;
    wxCheckBox* compareDateCheck_ = nullptr;
    wxCheckBox* twoWaySyncCheck_ = nullptr;
    wxCheckBox* compareSizeCheck_ = nullptr;
    wxCheckBox* copyWhenTargetNewerCheck_ = nullptr;
    wxCheckBox* compareContentCheck_ = nullptr;
    wxCheckBox* fatherSonPrincipleCheck_ = nullptr;
    wxCheckBox* deleteInTargetCheck_ = nullptr;
    wxCheckBox* backupProofCheck_ = nullptr;
    wxCheckBox* syncEmptyFilesCheck_ = nullptr;
    wxSpinCtrl* syncCycleSpin_ = nullptr;

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
    ID_SYNC_CYCLE_SPIN
};
