#include "SyncInterface.h"
#include "SyncControler.h"
#include "ProgressDialog.h"
#include "Synchronize.h"
#include "Menu.h"
#include "Config.h"
#include "Logging.h"
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

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
        syncControler_.register_progress_dialog(nullptr);
    }
}

bool SyncInterface::silentMode() const
{
    return consoleMode_ || Config::instance().settings().start_minimized();
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
    syncControler_.register_progress_dialog(progressDialog_);
}

void SyncInterface::syncOnceFinished()
{
    log_debug(L"SyncInterface::syncOnceFinished()");
    syncControler_.display_errormessages(false);
    syncControler_.destroy_synchronize();
    if (progressDialog_ != nullptr) {
        progressDialog_->canBeClosed(true);
        progressDialog_->Destroy(); // not delete: lets wx flush any events still targeting the window first
        progressDialog_ = nullptr;
        syncControler_.register_progress_dialog(nullptr);
    }
    menu_.syncMenu()->Enable(tul(id::StartSync), true);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    actSyncType_ = SyncType::None;
}

void SyncInterface::countFilesFinished()
{
    log_debug(L"SyncInterface::countFilesFinished()");
    syncControler_.destroy_synchronize();
    countFiles_ = 0;
    if (progressDialog_ != nullptr) {
        progressDialog_->canBeClosed(true);
        progressDialog_->Destroy(); // not delete: lets wx flush any events still targeting the window first
        progressDialog_ = nullptr;
        syncControler_.register_progress_dialog(nullptr);
    }
    countFiles_ = syncControler_.count_files();
    if (countFiles_ > 0) {
        actSyncType_ = SyncType::Single;
        startSync();
    }
    else {
        if (parentFrame())
            parentFrame()->SetStatusText("");
        wxMessageBox("Es wurden keine zu synchronisierenden Dateien gefunden.", "Info");
        actSyncType_ = SyncType::None;
        menu_.syncMenu()->Enable(tul(id::StartSync), true);  // [SyncInterface.md#terminal-outcome-reenables-menu]
        menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    }
}

void SyncInterface::continuousSyncFinished()
{
    syncControler_.destroy_synchronize();
    actSyncType_ = SyncType::None;
}

void SyncInterface::onTimer()
{
    log_debug(L"SyncInterface::onTimer()");
    if (!Config::exists() || !Config::instance().ready())
        return;
    if (syncControler_.sync_finished()) {
        if (actSyncType_ == SyncType::CountFiles)
            countFilesFinished();
        else if (actSyncType_ == SyncType::Single)
            syncOnceFinished();
        else if (actSyncType_ == SyncType::Continuous)
            continuousSyncFinished();
        syncControler_.reset_state();
    }
    if (actSyncType_ == SyncType::Continuous && !Synchronize::has_instance())
        onSync();
}

bool SyncInterface::singleSync()
{
    auto canSync = []() {
        return Config::instance().ready() && !Config::instance().syncitems().empty() && !Synchronize::has_instance();
    };
    if (canSync()) {
        syncControler_.display_errormessages(true);
        actSyncType_ = SyncType::CountFiles;
        return startSync();
    }
    return false;
}

bool SyncInterface::startSync()
{
    log_debug(L"SyncInterface::startSync()");
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
    if (!syncControler_.start_sync(actSyncType_)) {
        wxMessageBox("Synchronisation kann nicht gestartet werden.", "Fehler");
        return false;
    }
    return true;
}

void SyncInterface::runConsoleSync()
{
    log_debug(L"SyncInterface::runConsoleSync()");
    if (Config::instance().syncitems().empty() || Synchronize::has_instance())
        return;
    actSyncType_ = SyncType::Single;  // [SyncInterface.md#console-single-sync]
    if (!syncControler_.start_sync(actSyncType_))
        return;
    while (!syncControler_.sync_finished())  // [SyncInterface.md#poll-for-completion]
        wxMilliSleep(50);
    syncControler_.destroy_synchronize();
    syncControler_.reset_state();
}

void SyncInterface::onSync()
{
    if (Config::instance().syncitems().empty()) {
        if (!silentMode())
            wxMessageBox("Keine Dateien zum Synchronisieren angegeben!", "Achtung");
        return;
    }
    try {
        if (Synchronize::has_instance()) {
            wxMessageBox(L"Synchronisation läuft bereits.", "Fehler");
            return;
        }
        startSync();
    }
    catch (std::exception& ex) {
        if (!silentMode())
            wxMessageBox(ex.what(), "Fehler");
        log_error(ex.what());
    }
    catch (...) {
        if (!silentMode())
            wxMessageBox("Synchronisation fehlgeschlagen.", "Fehler");
        log_error("Synchronisation fehlgeschlagen.");
    }
}

void SyncInterface::startContinuousSync()
{
    log_debug(L"SyncInterface::startContinuousSync()");
    actSyncType_ = SyncType::Continuous;
    menu_.syncMenu()->Enable(tul(id::StartSync), false);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
    menu_.syncMenu()->Enable(tul(id::StopSync), true);
}

void SyncInterface::stopContinuousSync()
{
    log_debug(L"SyncInterface::stopContinuousSync()");
    menu_.syncMenu()->Enable(tul(id::StartSync), true);
    menu_.syncMenu()->Enable(tul(id::SyncOnce), true);
    menu_.syncMenu()->Enable(tul(id::StopSync), false);
    syncControler_.stop_sync();
}

void SyncInterface::stopSync()
{
    syncControler_.stop_sync();
}
