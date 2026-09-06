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
    SyncType        actSyncType_ = SyncType::Single;
    std::size_t     countFiles_ = 0;
    std::chrono::high_resolution_clock::time_point startTime_;
    bool silentMode() const;
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
    SyncControler& syncControler() { return syncControler_; }
    SyncType actSyncType() const { return actSyncType_; }
    void setActSyncType(SyncType type) { actSyncType_ = type; }
    bool isContinuouslySyncing() const { return actSyncType_ == SyncType::Continuous; }
};
