#include "pch.h"
#include "SyncTree.h"
#include "Global.h"
#include "SyncFileSsh.h"
#include "Util.h"
#include "Synchronize.h"
#include "Logging.h"
#include "CopyLog.h"
#include <filesystem>

namespace AFS_Core {

using std::wstring;
using namespace util;
namespace fs = std::filesystem;

namespace {
    wstring side_path(const wstring& path, bool ssh)
    {
        return ssh ? path : without_volume_spec(path);
    }
}

SyncTree::SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count)
    : SyncBase(si, copy_count, only_count)
{}

// SSH/SFTP paths always use '/', regardless of the client platform
wchar_t SyncTree::source_separator() const
{
    return sync_item_.ssh_source() ? L'/' : DIR_SEPARATOR;
}

wchar_t SyncTree::target_separator() const
{
    return sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR;
}

std::vector<SyncTree::Entry> SyncTree::list_source(const wstring& sourcename) const
{
    std::vector<Entry> entries;
    if (sync_item_.ssh_source()) {
        for (const SshDirEntry& entry : ssh_list_dir(sourcename))
            entries.push_back({entry.name, entry.is_directory, entry.is_regular_file, entry.is_symlink});
        return entries;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(with_trailing_char(sourcename, DIR_SEPARATOR))) {
        try {
            entries.push_back({entry.path().filename().wstring(), entry.is_directory(), entry.is_regular_file(), entry.is_symlink()});
        } catch (const std::exception& e) {
            // One unreadable entry (e.g. a name that cannot be converted) must not hide the rest of the directory
            log_error(sourcename, string_to_wstring(e.what()));
        }
    }
    return entries;
}

void SyncTree::sync_tree(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_tree: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;
    try {
        create_target_dir_if_not_exist(targetname);
        const std::set<wstring> source_names = sync_entries(sourcename, targetname, wildcard);
        // Target-only subdirectories are intentionally out of scope here (see
        // sync_target_only_entries()'s doc comment) - only files at this level.
        if (!Synchronize::stopping())
            sync_target_only_entries(sourcename, targetname, wildcard, source_names);
    }
    catch (const std::exception& e) {
        log_error(sourcename, targetname, string_to_wstring(e.what()));
        report_sync_error(sourcename + L": " + string_to_wstring(e.what()));
    }
    catch (...) {
        log_error(sourcename, targetname, L" - Unbekannter Fehler");
        report_sync_error(L"Unbekannter Fehler: " + sourcename);
    }
}

// Returns every name seen in this directory level's source listing, so the
// target-only pass can tell "not in source" from "wildcard-excluded, ignore" -
// see SyncBase::sync_target_only_entries(). Subdirectory names are included,
// so a target file sharing a source subdirectory's name is never mistaken for
// an orphan.
std::set<wstring> SyncTree::sync_entries(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    std::set<wstring> source_names;
    for (const Entry& entry : list_source(sourcename)) {
        source_names.insert(entry.name);
        sync_entry(entry, sourcename, targetname, wildcard);
        if (Synchronize::stopping())
            break;
    }
    return source_names;
}

void SyncTree::sync_entry(const Entry& entry, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    // Never follow a symlink to a directory: a link back to an ancestor would
    // recurse forever. An SSH listing cannot tell what a link points to without
    // an extra round trip, so there every symlink is skipped.
    if (entry.is_symlink && !entry.is_regular_file)
        log_info(L"Symlink wird nicht verfolgt: ", with_trailing_char(sourcename, source_separator()) + entry.name);
    else if (entry.is_directory)
        sync_subtree(entry.name, sourcename, targetname, wildcard);
    else if (entry.is_regular_file)
        sync_file_entry(entry.name, sourcename, targetname, wildcard);
}

void SyncTree::sync_subtree(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    // An excluded directory is skipped as a whole, not filtered per contained
    // file: a subtree like "node_modules" is never entered.
    if (is_excluded(name, sync_item_.exclusions()))
        log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + name);
    else
        sync_tree(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard);
}

void SyncTree::sync_file_entry(const wstring& name, const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    if ((!wildcard.empty() && !wildmatch(name, wildcard)) || is_excluded(name, sync_item_.exclusions()))
        return;
    sync_file(with_trailing_char(sourcename, source_separator()) + name, with_trailing_char(targetname, target_separator()) + name, wildcard);
}

void SyncTree::operator()()
{
    log_debug(L"SyncTree::operator()()");
    sync_tree(side_path(sync_item_.source(), sync_item_.ssh_source()), side_path(sync_item_.target(), sync_item_.ssh_target()), sync_item_.wildcard());
}

} // namespace AFS_Core
