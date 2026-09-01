#include "SyncInterface.h"
#include "SyncControler.h"
#include "ProgressDialog.h"
#include "Synchronize.h"
#include "Menu.h"
#include "config.h"
#include "Logging.h"
#include "id.h"
#include "utilwx.h"

using util::tul;

SyncInterface::SyncInterface(wxWindow& parentWindow, Menu& menu)
    : parentWindow_(parentWindow)
    , menu_(menu)
    , syncControler_(SyncControler::instance())
{
}

SyncInterface::~SyncInterface()
{
    if (progressDialog_ != nullptr) {
        progressDialog_->canBeClosed(true);
        progressDialog_->Destroy(); // not delete: lets wx flush any events still targeting the window first
        progressDialog_ = nullptr;
        syncControler_.registerProgressDialog(nullptr);
    }
}

bool SyncInterface::silentMode() const
{
    return consoleMode_ || Config::instance().settings().startMinimized();
}

void SyncInterface::setConsoleMode()
{
    consoleMode_ = true;
}

void SyncInterface::openProgressDialog(uint32_t nFiles)
{
    if (progressDialog_ != nullptr || silentMode())
        return;
    progressDialog_ = new ProgressDialog(&parentWindow_, wxID_ANY, _("Fortschritt"), syncControler_, nFiles);
    progressDialog_->Show();
    syncControler_.registerProgressDialog(progressDialog_);
}

void SyncInterface::syncOnceFinished()
{
    LOG_DEBUG(L"SyncInterface::syncOnceFinished()");
    syncControler_.displayErrormessages(false);
    syncControler_.destroySynchronize();
    if (progressDialog_ != nullptr) {
        progressDialog_->canBeClosed(true);
        progressDialog_->Destroy(); // not delete: lets wx flush any events still targeting the window first
        progressDialog_ = nullptr;
        syncControler_.registerProgressDialog(nullptr);
    }
    menu_.syncMenu()->Enable(tul(id::StartSync), true);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    actSyncType_ = SyncType::None;
}

void SyncInterface::countFilesFinished()
{
    LOG_DEBUG(L"SyncInterface::countFilesFinished()");
    syncControler_.destroySynchronize();
    countFiles_ = 0;
    if (progressDialog_ != nullptr) {
        progressDialog_->canBeClosed(true);
        progressDialog_->Destroy(); // not delete: lets wx flush any events still targeting the window first
        progressDialog_ = nullptr;
        syncControler_.registerProgressDialog(nullptr);
    }
    countFiles_ = syncControler_.countFiles();
    if (countFiles_ > 0) {
        actSyncType_ = SyncType::Single;
        startSync();
    }
    else {
        if (parentFrame())
            parentFrame()->SetStatusText("");
        wxMessageBox("Es wurden keine zu synchronisierenden Dateien gefunden.", "Info");
        actSyncType_ = SyncType::None;
        // This is a terminal outcome (no startSync()/syncOnceFinished() follows to
        // do it): re-enable what onStartSingleSync() disabled, or the menu stays
        // greyed out forever whenever a sync legitimately finds nothing to do.
        menu_.syncMenu()->Enable(tul(id::StartSync), true);
        menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    }
}

void SyncInterface::continuousSyncFinished()
{
    syncControler_.destroySynchronize();
    actSyncType_ = SyncType::None;
}

void SyncInterface::onTimer()
{
    LOG_DEBUG(L"SyncInterface::onTimer()");
    if (!Config::exists() || !Config::instance().ready())
        return;
    if (syncControler_.syncFinished()) {
        if (actSyncType_ == SyncType::CountFiles)
            countFilesFinished();
        else if (actSyncType_ == SyncType::Single)
            syncOnceFinished();
        else if (actSyncType_ == SyncType::Continuous)
            continuousSyncFinished();
        syncControler_.resetState();
    }
    if (actSyncType_ == SyncType::Continuous && !Synchronize::hasInstance())
        onSync();
}

bool SyncInterface::singleSync()
{
    auto canSync = []() {
        return Config::instance().ready() && !Config::instance().syncitems().empty() && !Synchronize::hasInstance();
    };
    if (canSync()) {
        syncControler_.displayErrormessages(true);
        actSyncType_ = SyncType::CountFiles;
        return startSync();
    }
    return false;
}

bool SyncInterface::startSync()
{
    LOG_DEBUG(L"SyncInterface::startSync()");
    if (actSyncType_ == SyncType::CountFiles) {
        if (parentFrame())
            parentFrame()->SetStatusText(L"Zähle Dateien für Synchronization ...");
        wxBusyCursor busyCursor;
        wxYield();
    }
    else if (actSyncType_ == SyncType::Single) {
        if (countFiles_ == 0) {
            if (parentFrame())
                parentFrame()->SetStatusText("");
            wxMessageBox("Es wurden keine zu synchronisierenden Dateien gefunden.", "Info");
            return false;
        }
        if (parentFrame())
            parentFrame()->SetStatusText("");
        openProgressDialog(countFiles_);
    }
    if (!syncControler_.startSync(actSyncType_)) {
        wxMessageBox("Synchronisation kann nicht gestartet werden.", "Fehler");
        return false;
    }
    return true;
}

void SyncInterface::runConsoleSync()
{
    LOG_DEBUG(L"SyncInterface::runConsoleSync()");
    if (Config::instance().syncitems().empty() || Synchronize::hasInstance())
        return;
    // Bypasses the GUI's count-then-sync two-phase dance (built for a progress
    // dialog's file-count range) and startSync()'s unconditional wxMessageBox
    // for countFiles_ == 0 - both would otherwise leave a --console
    // invocation (a cron/systemd job with nobody watching) stuck on a modal
    // dialog it can never dismiss, without ever having synced anything.
    actSyncType_ = SyncType::Single;
    if (!syncControler_.startSync(actSyncType_))
        return;
    // Poll for actual completion instead of calling stopSync(): that requests
    // an early stop, aborting mid-item and syncing only whatever the worker
    // thread happened to reach in the few CPU cycles before this call.
    while (!syncControler_.syncFinished())
        wxMilliSleep(50);
    syncControler_.destroySynchronize();
    syncControler_.resetState();
}

void SyncInterface::onSync()
{
    if (Config::instance().syncitems().empty()) {
        if (!silentMode())
            wxMessageBox("Keine Dateien zum Synchronisieren angegeben!", "Achtung");
        return;
    }
    try {
        if (Synchronize::hasInstance()) {
            wxMessageBox(L"Synchronisation läuft bereits.", "Fehler");
            return;
        }
        startSync();
    }
    catch (std::exception& ex) {
        if (!silentMode())
            wxMessageBox(ex.what(), "Fehler");
        LOG_ERROR(ex.what());
    }
    catch (...) {
        if (!silentMode())
            wxMessageBox("Synchronisation fehlgeschlagen.", "Fehler");
        LOG_ERROR("Synchronisation fehlgeschlagen.");
    }
}

void SyncInterface::startContinuousSync()
{
    LOG_DEBUG(L"SyncInterface::startContinuousSync()");
    actSyncType_ = SyncType::Continuous;
    menu_.syncMenu()->Enable(tul(id::StartSync), false);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
    menu_.syncMenu()->Enable(tul(id::StopSync), true);
}

void SyncInterface::stopContinuousSync()
{
    LOG_DEBUG(L"SyncInterface::stopContinuousSync()");
    menu_.syncMenu()->Enable(tul(id::StartSync), true);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    menu_.syncMenu()->Enable(tul(id::StopSync), false);
    syncControler_.stopSync();
}

void SyncInterface::stopSync()
{
    syncControler_.stopSync();
}
