#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "SyncItem.h"
#include "Global.h"

namespace AFS_Core {

class SyncReport {
    uint64_t        bytes_transfered_ = 0;
    WStringVector   errors_;
public:
    static constexpr std::size_t MAX_ERRORS = 100;
    void transferred(uint64_t bytes) { bytes_transfered_ += bytes; }
    void error(const std::wstring& message);
    uint64_t bytes_transferd() const { return bytes_transfered_; }
    const WStringVector& errors() const { return errors_; }
    bool successful() const { return errors_.empty(); }
    bool empty() const { return bytes_transfered_ == 0 && errors_.empty(); }
};

// Makes report the one that report_sync_error()/report_sync_volume() feed, for as
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
void report_sync_error(const std::wstring& message);
void report_sync_volume(uint64_t bytes);

// Appends one entry to <directory>/<sanitised item name>.json, creating the
// file and the directory as needed. Returns false if the entry did not reach
// the file - which is worth logging, since a missing proof is the one thing
// this feature must not do quietly.
bool write_copy_log(const std::filesystem::path& directory, const SyncItem& item,
    std::size_t files, const SyncReport& report);

// The proof file that write_copy_log() appends an item's entries to.
std::filesystem::path copy_log_file_path(const std::filesystem::path& directory,
    const std::wstring& item_name);

} // namespace AFS_Core
