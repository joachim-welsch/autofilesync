#include "pch.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include "Util.h"
#include "Logging.h"
#include "Global.h"
#include "CopyLog.h"

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
    if (!entry.is_regular_file || (!wildcard.empty() && !wildmatch(entry.name, wildcard)) || is_excluded(entry.name, sync_item_.exclusions()))
        return;
    // source_path is always SSH here (this method only runs for an SSH source);
    // destination may still be a local path, so only it depends on ssh_target().
    const wstring fname = util::with_trailing_char(source_path, L'/') + entry.name;
    const wstring dest = util::with_trailing_char(destination, sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR) + entry.name;
    sync_file(fname, dest, wildcard);
}

void SyncDir::operator()()
{
    log_debug(L"SyncDir::operator()()");
    const wstring source = sync_item_.source();
    if (source.empty())
        return;
    const wstring destination = sync_item_.target();
    const wstring wildcard = sync_item_.wildcard();
    // Names seen in the source, so the target-only pass below can tell "not
    // in source" from "wildcard-excluded, ignore" - see sync_target_only_entries().
    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist();
        if (sync_item_.ssh_source()) {
            for (const auto& entry : ssh_list_dir(source)) {
                source_names.insert(entry.name);
                sync_ssh_dir_entry(entry, source, wildcard, destination);
                if (stop_requested())
                    break;
            }
        } else {
            const wstring local_dest = sync_item_.ssh_target() ? destination : without_volume_spec(destination);
            for (const fs::directory_entry& dir_entry : fs::directory_iterator(without_volume_spec(source))) {
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
        log_error(source, destination, string_to_wstring(e.what()));
        report_sync_error(source + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        log_error(source, destination, L" - Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler: " + source);
    }
}

} // namespace AFS_Core
