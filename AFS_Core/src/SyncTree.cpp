#include "pch.h"
#include "SyncTree.h"
#include "Global.h"
#include "SyncDir.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "Util.h"
#include "Synchronize.h"
#include "Logging.h"
#include "CopyLog.h"
#include <filesystem>
#include <iostream>


namespace AFS_Core {

using std::wstring;
using namespace util;
namespace fs = std::filesystem;
using std::exception;

SyncTree::SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count)
    : SyncBase(si, copy_count, only_count)
{}

void SyncTree::count(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"count: " + sourcename + L" -> " + targetname);
    if (sync_item_.ssh_source()) {
        sync_ssh_branch(sourcename, targetname, wildcard);
    } else {
        sync_branch(without_volume_spec(sourcename), without_volume_spec(targetname), wildcard);
    }
}

void SyncTree::sync_ssh_dir_entry(const SshDirEntry& entry, const wstring& source_path, const wstring& wildcard, const wstring& destination)
{
    log_debug(L"sync_ssh_dir_entry: " + entry.name);
    
    // directories are handled by sync_ssh_branch
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

    wstring fname = util::with_trailing_char(source_path, L'/') + entry.name;
    wstring dest = util::with_trailing_char(destination, L'/') + entry.name;
    
    SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), fname, dest, wildcard, Exclusions(), sync_item_.flags(), sync_item_.compare_items()), copy_count_)();
}

void SyncTree::sync_ssh_branch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_ssh_branch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;
    // Names seen at this directory level's source listing, so the
    // target-only pass below can tell "not in source" from "wildcard-
    // excluded, ignore" - see SyncBase::sync_target_only_entries(). Includes
    // subdirectory names too, so a target file that happens to share a
    // source subdirectory's name is never mistaken for an orphan.
    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist(targetname);

        auto entries = ssh_list_dir(sourcename);
        for (const auto& entry : entries) {
            if (entry.name == L"." || entry.name == L"..") {
                continue;
            }
            source_names.insert(entry.name);

            // Never follow SSH symlinks - see sync_branch()'s local-path check
            // above for the same reasoning (a link back to an ancestor would
            // recurse forever). Unlike the local path, this can't safely
            // distinguish "symlink to a directory" from "symlink to a file"
            // without an extra round trip per entry, so every symlink is
            // skipped rather than synced - conservative, but correct either way.
            if (entry.is_symlink) {
                log_info(L"SSH-Symlink wird nicht verfolgt: ", with_trailing_char(sourcename, L'/') + entry.name);
            } else if (entry.is_directory) {
                // An excluded directory is skipped as a whole, not filtered per
                // contained file: a subtree like "node_modules" is never entered.
                if (is_excluded(entry.name, sync_item_.exclusions())) {
                    log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + entry.name);
                } else {
                    wstring source = with_trailing_char(sourcename, L'/') + entry.name;
                    wstring target = with_trailing_char(targetname, L'/') + entry.name;
                    sync_ssh_branch(source, target, wildcard);
                }
            } else {
                sync_ssh_dir_entry(entry, sourcename, wildcard, targetname);
            }

            if (Synchronize::stopping())
                break;
        }
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

void SyncTree::sync_branch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_branch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;
    SyncItem sync_item(L"", SyncItemType::Dir, sourcename, targetname, wildcard, sync_item_.exclusions(), sync_item_.flags(),
            sync_item_.compare_items());
    wstring search = with_trailing_char(sourcename, DIR_SEPARATOR);
    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist(targetname);
        for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(search)) {
            const auto& newpath = dir_entry.path();
            if (newpath.has_filename()) {
                wstring filename;
                try {
                    filename = newpath.filename().wstring();
                } catch (const exception& e) {
                    log_error(sourcename, targetname, string_to_wstring(e.what()));
                    continue;
                }
                if (filename != L"." && filename != L"..")
                    source_names.insert(filename);
                if (dir_entry.is_directory() && filename != L"." && filename != L"..") {
                    // Never follow directory symlinks: a link back to an ancestor
                    // (or any other cycle) would otherwise make this recurse
                    // forever, exhausting the stack.
                    if (dir_entry.is_symlink()) {
                        log_info(L"Symlink-Verzeichnis wird nicht verfolgt: ", with_trailing_char(sourcename, DIR_SEPARATOR) + filename);
                    } else if (is_excluded(filename, sync_item_.exclusions())) {
                        // Skip the whole subtree rather than filtering per file.
                        log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + filename);
                    } else {
                        wstring target = with_trailing_char(targetname, DIR_SEPARATOR) + filename;
                        wstring source = with_trailing_char(sourcename, DIR_SEPARATOR) + filename;
                        sync_branch(source, target, wildcard);
                    }
                } else
                    sync_dir_entry(dir_entry, wildcard, targetname);
            }
            if (Synchronize::stopping())
                break;
        }
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

void SyncTree::sync_local_to_ssh_branch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_local_to_ssh_branch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;

    wstring search = with_trailing_char(sourcename, DIR_SEPARATOR);
    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist(targetname);
        for (const fs::directory_entry& dir_entry : std::filesystem::directory_iterator(search)) {
            const auto& newpath = dir_entry.path();
            if (newpath.has_filename()) {
                wstring filename;
                try {
                    filename = newpath.filename().wstring();
                } catch (const exception& e) {
                    log_error(sourcename, targetname, string_to_wstring(e.what()));
                    continue;
                }
                if (filename != L"." && filename != L"..")
                    source_names.insert(filename);
                if (dir_entry.is_directory() && filename != L"." && filename != L"..") {
                    // Never follow directory symlinks - see sync_branch() above.
                    if (dir_entry.is_symlink()) {
                        log_info(L"Symlink-Verzeichnis wird nicht verfolgt: ", with_trailing_char(sourcename, DIR_SEPARATOR) + filename);
                    } else if (is_excluded(filename, sync_item_.exclusions())) {
                        log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + filename);
                    } else {
                        // SSH-Ziel: POSIX '/' verwenden
                        wstring target = with_trailing_char(targetname, L'/') + filename;
                        wstring source = with_trailing_char(sourcename, DIR_SEPARATOR) + filename;
                        sync_local_to_ssh_branch(source, target, wildcard);
                    }
                } else if (dir_entry.is_regular_file()) {
                    // Wildcard-Matching prüfen
                    if (!wildcard.empty() && !wildmatch(filename, wildcard)) {
                        continue;
                    }
                    if (is_excluded(filename, sync_item_.exclusions())) {
                        continue;
                    }

                    if (only_count_) {
                        copy_count_++;
                    } else {
                        wstring source = with_trailing_char(sourcename, DIR_SEPARATOR) + filename;
                        wstring dest = with_trailing_char(targetname, L'/') + filename;
                        SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, wildcard,
                                 Exclusions(), sync_item_.flags(), sync_item_.compare_items()), copy_count_)();
                    }
                }
            }
            if (Synchronize::stopping())
                break;
        }
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

void SyncTree::sync_ssh_to_ssh_branch(const wstring& sourcename, const wstring& targetname, const wstring& wildcard)
{
    log_debug(L"sync_ssh_to_ssh_branch: " + sourcename + L" -> " + targetname);
    if (Synchronize::stopping())
        return;

    std::set<wstring> source_names;
    try {
        create_target_dir_if_not_exist(targetname);

        auto entries = ssh_list_dir(sourcename);
        for (const auto& entry : entries) {
            if (entry.name == L"." || entry.name == L"..") {
                continue;
            }
            source_names.insert(entry.name);

            // Never follow SSH symlinks - see sync_ssh_branch() above.
            if (entry.is_symlink) {
                log_info(L"SSH-Symlink wird nicht verfolgt: ", with_trailing_char(sourcename, L'/') + entry.name);
            } else if (entry.is_directory) {
                if (is_excluded(entry.name, sync_item_.exclusions())) {
                    log_debug(L"Ausgeschlossenes Verzeichnis wird uebersprungen: " + entry.name);
                } else {
                    // Rekursiv in Unterverzeichnis absteigen (beide SSH-Pfade: POSIX '/')
                    wstring source = with_trailing_char(sourcename, L'/') + entry.name;
                    wstring target = with_trailing_char(targetname, L'/') + entry.name;
                    sync_ssh_to_ssh_branch(source, target, wildcard);
                }
            } else if (entry.is_regular_file) {
                // Wildcard-Matching prüfen
                if (!wildcard.empty() && !wildmatch(entry.name, wildcard)) {
                    continue;
                }
                if (is_excluded(entry.name, sync_item_.exclusions())) {
                    continue;
                }

                if (only_count_) {
                    copy_count_++;
                } else {
                    wstring source = with_trailing_char(sourcename, L'/') + entry.name;
                    wstring dest = with_trailing_char(targetname, L'/') + entry.name;
                    SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source, dest, wildcard,
                             Exclusions(), sync_item_.flags(), sync_item_.compare_items()), copy_count_)();
                }
            }

            if (Synchronize::stopping())
                break;
        }
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

void SyncTree::operator()()
{
	log_debug(L"SyncTree::operator()()");

	if (sync_item_.ssh_source() && sync_item_.ssh_target()) {
		log_debug(L"SyncTree SSH-zu-SSH: " + sync_item_.source() + L" -> " + sync_item_.target());
		sync_ssh_to_ssh_branch(sync_item_.source(), sync_item_.target(), sync_item_.wildcard());
	}
	else if (sync_item_.ssh_source()) {
		// SSH-Quelle: Verwende SSH-basiertes Directory Listing
		log_debug(L"SyncTree mit SSH-Quelle: " + sync_item_.source());
		sync_ssh_branch(sync_item_.source(), 
					  sync_item_.ssh_target() ? sync_item_.target() : without_volume_spec(sync_item_.target()), 
					  sync_item_.wildcard());
	}
	else if (sync_item_.ssh_target()) {
		// Lokale Quelle, SSH-Ziel
		log_debug(L"SyncTree lokal->SSH: " + sync_item_.source() + L" -> " + sync_item_.target());
		sync_local_to_ssh_branch(without_volume_spec(sync_item_.source()), sync_item_.target(), sync_item_.wildcard());
	}
	else {
		// Lokale Quelle
		sync_branch(without_volume_spec(sync_item_.source()), 
				   sync_item_.ssh_target() ? sync_item_.target() : without_volume_spec(sync_item_.target()), 
				   sync_item_.wildcard());
	}
}

} // namespace AFS_Core
