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
#include <wx/wx.h>
#endif

#include <cstdint>
#include <chrono>
#include "Global.h"

namespace AFS_Core { class SyncControler; }
class ProgressDialog;
class Menu;

using namespace AFS_Core;

class SyncInterface {
    wxWindow&       parentWindow_;
    Menu&           menu_;
    SyncControler&  syncControler_;
    ProgressDialog* progressDialog_ = nullptr;
    bool            consoleMode_ = false;
    SyncType        actSyncType_ = SyncType::None;  // [SyncInterface.md#stateText]
    std::size_t     countFiles_ = 0;
    std::chrono::high_resolution_clock::time_point startTime_;
    bool silentMode() const;
    void showRunMessages();  // [SyncInterface.md#showRunMessages]
    wxFrame* parentFrame() { return dynamic_cast<wxFrame*>(&parentWindow_); }
public:
    explicit SyncInterface(wxWindow& parentWindow, Menu& menu);
    ~SyncInterface();
    void onSync();
    void runConsoleSync();
    bool startSync();
    bool singleSync();
    void setConsoleMode();
    void openProgressDialog(uint32_t nFiles);
    void stopSync();
    void syncOnceFinished();
    void countFilesFinished();
    void continuousSyncFinished();
    void onTimer();
    void startContinuousSync();
    void stopContinuousSync();
    wxString stateText() const;  // [SyncInterface.md#stateText]
    SyncControler& syncControler() { return syncControler_; }
    SyncType actSyncType() const { return actSyncType_; }
    void setActSyncType(SyncType type) { actSyncType_ = type; }
    bool isContinuouslySyncing() const { return actSyncType_ == SyncType::Continuous; }
};
