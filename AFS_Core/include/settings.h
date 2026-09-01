#pragma once
#include <stdint.h>
#include <string>

enum class DeleteMode { RecycleBin, Permanent };

class Settings {
    // Recoverable deletion is the safer default and also what a config file
    // written before this setting existed falls back to.
    DeleteMode deleteMode_          = DeleteMode::RecycleBin;
    bool consoleMode_               = false;
    bool startMinimized_            = false;
    bool minimizeInsteadOfClose_    = false;
    bool autoStart_                 = false;
    bool startSyncAtStart_          = false;
    bool showErrors_                = false;
    bool showWarnings_              = false;
    bool showInfos_                 = false;
    // Directory the proofs of backup go into, one file per sync item. Empty
    // means none are written, whatever a single sync item asks for - there is
    // nowhere to put them.
    std::wstring backupProofPath_;
public:
    void setConsoleMode() { consoleMode_=true; }
    bool consoleMode() const { return consoleMode_; }
    bool startMinimized() const { return startMinimized_; }
    void startMinimized(bool sm) { startMinimized_=sm; }
    bool minimizeInsteadOfClose() const { return minimizeInsteadOfClose_; }
    void minimizeInsteadOfClose(bool m) { minimizeInsteadOfClose_ = m; }
    bool autoStart() const { return autoStart_; }
    void autoStart(bool as) { autoStart_=as; }
    bool startSyncAtStart() const { return startSyncAtStart_; }
    void startSyncAtStart(bool as) { startSyncAtStart_=as; }
    bool showErrors() const { return showErrors_; }
    void showErrors(bool t) { showErrors_ = t; }
    bool showWarnings() const { return showWarnings_; }
    void showWarnings(bool t) { showWarnings_ = t; }
    bool showInfos() const { return showInfos_; }
    void showInfos(bool t) { showInfos_ = t; }
    DeleteMode deleteMode() const { return deleteMode_; }
    void deleteMode(DeleteMode dm) { deleteMode_ = dm; }
    std::wstring backupProofPath() const { return backupProofPath_; }
    void backupProofPath(const std::wstring& path) { backupProofPath_ = path; }
    void reset();
};
