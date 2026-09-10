#include "pch.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "Util.h"
#include "Logging.h"
#include "Global.h"
#include "CopyLog.h"
#include <iostream>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncDir::SyncDir(const SyncItem& si, std::size_t& copy_count, bool only_count)
    : SyncBase(si, copy_count, only_count)
{}

void SyncDir::sync_ssh_dir_entry(const SshDirEntry& entry, const wstring& source_path, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_ssh_dir_entry: " + entry.name);
    
    if (!entry.is_regular_file) {
        return;
    }
    
    // Wildcard-Matching prüfen
    if (!wildcard.empty() && !wildmatch(entry.name, wildcard)) {
        return;
    }
    if (is_excluded(entry.name, sync_item_.exclusions())) {
        return;
    }

    if (only_count_) {
        copy_count_++;
        return;
    }

    // source_path is always SSH here (this method only runs for an SSH source);
    // destination may still be a local path, so only it depends on ssh_target().
    wstring fname = util::with_trailing_char(source_path, L'/') + entry.name;
    wstring dest = util::with_trailing_char(destination, sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR) + entry.name;
    
    SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), fname, dest, wildcard, Exclusions(), sync_item_.flags(), sync_item_.compare_items()), copy_count_)();
}

void SyncDir::operator()()
{
    log_debug(L"SyncDir::operator()()");
    wstring source = sync_item_.source();
    if (source.empty())
        return;
    wstring destination = sync_item_.target();
    const wstring wildcard = sync_item_.wildcard();
    
    // Names seen in the source, so the target-only pass below can tell "not
    // in source" from "wildcard-excluded, ignore" - see sync_target_only_entries().
    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist();

        // SSH-Quelle behandeln
        if (sync_item_.ssh_source()) {
            log_debug(L"SyncDir mit SSH-Quelle: " + source);
            auto entries = ssh_list_dir(source);
            for (const auto& entry : entries) {
                source_names.insert(entry.name);
                sync_ssh_dir_entry(entry, source, wildcard, destination);
                if (stop_requested())
                    break;
            }
        } else {
            // Lokale Quelle
            wstring local_source = without_volume_spec(source);
            wstring local_dest = sync_item_.ssh_target() ? destination : without_volume_spec(destination);
            for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(local_source)) {
                if (dir_entry.path().has_filename())
                    source_names.insert(dir_entry.path().filename().wstring());
                sync_dir_entry(dir_entry, wildcard, local_dest);
                if (stop_requested())
                    break;
            }
        }
        if (!stop_requested())
            sync_target_only_entries(source, destination, wildcard, source_names);
    }
    catch (const std::exception& e) {
        log_error(string_to_wstring(e.what()));
        report_sync_error(string_to_wstring(e.what()));
    }
}

} // namespace AFS_Core
