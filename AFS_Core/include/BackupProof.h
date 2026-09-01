#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "syncitem.h"

// What one synchronisation pass over one sync item did, as far as a proof of
// backup has to state it. The file count is not kept here: it comes from the
// same counter the rest of the program reports (Synchronize::countFiles()), so
// that the proof cannot disagree with what the user is shown.
class SyncReport {
    uint64_t bytes_ = 0;
    std::vector<std::wstring> errors_;
public:
    // A tree that fails file by file would otherwise grow the report - and with
    // it the proof file - without limit. The message below is appended once
    // when the limit is reached, so a truncated list never reads as a complete one.
    static constexpr std::size_t maxErrors = 100;
    void transferred(uint64_t bytes) { bytes_ += bytes; }
    void error(const std::wstring& message);
    uint64_t bytes() const { return bytes_; }
    const std::vector<std::wstring>& errors() const { return errors_; }
    bool successful() const { return errors_.empty(); }
    bool empty() const { return bytes_ == 0 && errors_.empty(); }
};

// Makes report the one that reportSyncError()/reportSyncVolume() feed, for as
// long as the guard lives. Per thread, because the GUI and the daemon each run
// their own sync thread (see Synchronize::create()).
class SyncReportGuard {
    SyncReport* previous_;
public:
    explicit SyncReportGuard(SyncReport* report);
    ~SyncReportGuard();
    SyncReportGuard(const SyncReportGuard&) = delete;
    SyncReportGuard& operator=(const SyncReportGuard&) = delete;
};

// No-ops while no proof is being collected, so the call sites deep in the sync
// classes need no condition of their own.
void reportSyncError(const std::wstring& message);
void reportSyncVolume(uint64_t bytes);

// Appends one entry to <directory>/<sanitised item name>.json, creating the
// file and the directory as needed. Returns false if the entry did not reach
// the file - which is worth logging, since a missing proof is the one thing
// this feature must not do quietly.
bool writeBackupProof(const std::filesystem::path& directory, const SyncItem& item,
    std::size_t files, const SyncReport& report);

// The proof file that writeBackupProof() appends an item's entries to.
std::filesystem::path backupProofFilePath(const std::filesystem::path& directory,
    const std::wstring& itemName);
