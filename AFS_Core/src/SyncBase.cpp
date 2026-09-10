#include "pch.h"
#include "SyncBase.h"
#include "CopyLog.h"
#include "Global.h"
#include "Logging.h"
#include "MountedDrives.h"
#include "SyncFile.h"
#include "SyncFileSsh.h"
#include "Synchronize.h"
#include "Util.h"
#include <filesystem>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using namespace util;

SyncBase::SyncBase(const SyncItem &sync_item, std::size_t &copy_count,
                   bool only_count)
    : sync_item_(sync_item), copy_count_(copy_count), only_count_(only_count) {}

bool SyncBase::drive_not_exist(const wstring &pathname, bool ssh)
{
  log_debug(L"drive_not_exist: " + pathname);
  if (ssh)
    return false; // TODO SSH-Laufwerke prüfen
  bool retval = !MountedDrives::instance().drive_mounted(pathname);
  if (!retval)
    log_debug(L"drive_not_exist: TRUE");
  return retval;
}

void SyncBase::create_target_dir_if_not_exist(const std::wstring &targetdir)
{
  log_debug(L"create_target_dir_if_not_exist: " + targetdir);
  wstring destination;
  if (targetdir.empty())
    destination = sync_item_.target();
  else
    destination = targetdir;
  if (only_count_)
    return;
  if (sync_item_.ssh_target()) {
    if (!ssh_exists(destination)) {
      if (!ssh_mkdir(destination)) {
        log_error(L"Fehler beim Erstellen des SSH-Verzeichnisses: " +
                  destination);
        report_sync_error(L"SSH-Zielverzeichnis nicht anlegbar: " + destination);
      }
    }
  } else { // local target
    wstring local_dest = without_volume_spec(destination);
    if (!fs::exists(local_dest)) {
      try {
        fs::create_directories(local_dest);
      }
      catch (std::filesystem::filesystem_error const &ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
      }
    }
  }
}

void SyncBase::sync_dir_entry(const fs::directory_entry &dir_entry,
                            const wstring &wildcard,
                            const wstring &destination)
{
  log_debug(L"sync_dir_entry: " + dir_entry.path().wstring());
  // SyncDir calls this for every entry unfiltered (SyncTree already filters
  // directories out before calling this at all). Without this check, a
  // subdirectory reached fs::file_size() inside FileDifference, which fails
  // for a directory - misread as "source file not found", a harmless no-op
  // normally, but one that could delete a same-named target *file* if
  // delete_in_target is enabled for this (non-File) item.
  if (dir_entry.is_directory())
    return;
  const auto &newpath = dir_entry.path();
  if (newpath.has_filename() &&
      (wildcard.empty() || wildmatch(newpath.filename().wstring(), wildcard)) &&
      !is_excluded(newpath.filename().wstring(), sync_item_.exclusions())) {
    if (only_count_) {
      copy_count_++;
      log_debug(L"sync_dir_entry count only");
      return;
    }
    wstring filename = newpath.filename().wstring();
    wstring fname = newpath.wstring();
    // SSH/SFTP destinations always use '/', regardless of the client platform;
    // only a local destination uses the platform separator.
    wchar_t separator = sync_item_.ssh_target() ? L'/' : DIR_SEPARATOR;
    wstring dest = util::with_trailing_char(destination, separator) + filename;
    try {
      SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), fname, dest,
                        wildcard, Exclusions(), sync_item_.flags(),
                        sync_item_.compare_items()),
               copy_count_)();
    } catch (const std::exception &e) {
      log_error(fname, dest, string_to_wstring(e.what()));
      report_sync_error(fname + L": " + string_to_wstring(e.what()));
    } catch (...) {
      log_error(fname, dest, L" - Unbekannter Fehler");
      report_sync_error(L"Unbekannter Fehler: " + fname);
    }
  } else {
    log_debug("newpath.has_filename()", newpath.has_filename());
    log_debug("wildcard.empty()", wildcard.empty());
    log_debug("wildmatch(newpath.filename().wstring(), wildcard)",
               wildmatch(newpath.filename().wstring(), wildcard));
  }
}

void SyncBase::sync_target_only_entries(const wstring &source_dir,
                                     const wstring &target_dir,
                                     const wstring &wildcard,
                                     const std::set<wstring> &source_names)
{
  // Cheap to skip entirely when neither option can act on what this would find.
  if (!sync_item_.delete_in_target() && !sync_item_.two_way_sync())
    return;

  auto handle_entry = [&](const wstring &name, const wstring &source_path,
                         const wstring &target_path) {
    if (source_names.count(name) ||
        (!wildcard.empty() && !wildmatch(name, wildcard)) ||
        is_excluded(name, sync_item_.exclusions()))
      return;
    if (only_count_) {
      copy_count_++;
      return;
    }
    try {
      // source_path is known not to exist (name absent from sourceNames):
      // SyncFile's own FileDifference-based decision (SourceNotExits ->
      // delete if delete_in_target, or copy back if two_way_sync) does the
      // rest, exactly as it already does for the source-to-target case.
      SyncFile(SyncItem(sync_item_.name(), sync_item_.sync_item_type(), source_path,
                        target_path, wildcard, Exclusions(), sync_item_.flags(),
                        sync_item_.compare_items()),
               copy_count_)();
    } catch (const std::exception &e) {
      log_error(source_path, target_path, string_to_wstring(e.what()));
      report_sync_error(target_path + L": " + string_to_wstring(e.what()));
    } catch (...) {
      log_error(source_path, target_path, L" - Unbekannter Fehler");
      report_sync_error(L"Unbekannter Fehler: " + target_path);
    }
  };

  const wchar_t source_separator = sync_item_.ssh_source() ? L'/' : DIR_SEPARATOR;
  if (sync_item_.ssh_target()) {
    for (const auto &entry : ssh_list_dir(target_dir)) {
      if (!entry.is_regular_file)
        continue;
      wstring target_path = util::with_trailing_char(target_dir, L'/') + entry.name;
      wstring source_path =
          util::with_trailing_char(source_dir, source_separator) + entry.name;
      handle_entry(entry.name, source_path, target_path);
      if (stop_requested())
        break;
    }
  } else {
    wstring local_target_dir = without_volume_spec(target_dir);
    if (!fs::exists(local_target_dir))
      return;
    for (const fs::directory_entry &dir_entry :
         fs::directory_iterator(local_target_dir)) {
      if (!dir_entry.is_regular_file())
        continue;
      wstring name = dir_entry.path().filename().wstring();
      wstring source_path =
          util::with_trailing_char(source_dir, source_separator) + name;
      handle_entry(name, source_path, dir_entry.path().wstring());
      if (stop_requested())
        break;
    }
  }
}

bool SyncBase::stop_requested() const { return Synchronize::stopping(); }

} // namespace AFS_Core
