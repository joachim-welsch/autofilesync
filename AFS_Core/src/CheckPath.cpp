#include "pch.h"
#include "CheckPath.h"
#include <sstream>
#include "Logging.h"
#include "Util.h"
#include "MountedDrives.h"
#include "Config.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include <FilesystemUtil.h>
#include <filesystem>
#include <optional>

namespace AFS_Core {

using namespace util;
using std::wstring;
namespace fs = std::filesystem;

namespace {
    constexpr uint16_t BASE_INTERVAL = 2;
    // Reachable: re-checking rarely is cheap and does not change the outcome.
    constexpr uint16_t MAX_INTERVAL_REACHABLE = 3600;
    // Unreachable: the check has to come back soon. With an hour of backoff the
    // sync would stay blind long after a late mount (boot race) or a network
    // change brought the target back.
    constexpr uint16_t MAX_INTERVAL_UNREACHABLE = 300;

    // A path belongs to an SSH connection if the text before the first colon is
    // the name of a configured account. A drive letter ("C:\...") looks similar
    // but finds no account.
    std::optional<SshAccount> ssh_account_of_path(const wstring& path, wstring& remote_path)
    {
        const auto colon_pos = path.find(L':');
        if (colon_pos == wstring::npos || colon_pos == 0)
            return std::nullopt;
        const wstring account_name = path.substr(0, colon_pos);
        auto account = Config::instance().find_ssh_account_by_name(wstring_to_string(account_name));
        if (!account.has_value())
            return std::nullopt;
        remote_path = path.substr(colon_pos + 1);
        return account;
    }
}

bool path_contains_volume_name(const wstring& path)
{
    const auto open_parenthesis_pos = path.find(L'(');
    const auto colon_pos = path.find(L':');
    const bool retval = open_parenthesis_pos != wstring::npos && colon_pos != wstring::npos && open_parenthesis_pos < colon_pos;
    log_debug("path_contains_volume_name " + wstring_to_string(path) + ": ", retval);
    return retval;
}

wstring volume_name(const wstring& path)
{
    log_debug(L"volume_name() " + path);
    if (!path_contains_volume_name(path))
        return {};
    return path.substr(0, path.find(L'(') - 1);
}

uint16_t next_check_interval(uint16_t previous_interval, bool repeats_previous_result, bool reachable)
{
    if (!repeats_previous_result)
        return BASE_INTERVAL;
    const uint16_t max_interval = reachable ? MAX_INTERVAL_REACHABLE : MAX_INTERVAL_UNREACHABLE;
    const uint32_t doubled = static_cast<uint32_t>(previous_interval) * 2u;
    return doubled > max_interval ? max_interval : static_cast<uint16_t>(doubled);
}

bool CheckPath::path_exists(const wstring& pathname, bool must_exist) const
{
    log_debug(L"drivesExists()");
#ifdef _UNIX
    (void)must_exist;
    std::error_code ec;
    return fs::exists(pathname, ec);
#else
    const wstring drive_name = root_name(pathname);
    if (path_contains_volume_name(pathname) && !volume_exists(drive_name, volume_name(pathname)))
        return false;
    // drive_exists() only ever saw the drive letter, never the path itself: a
    // source folder deleted underneath an existing drive still counted as
    // reachable, so the pass fell through to SyncDir, whose exception got
    // reported unconditionally on every single pass instead of once (see
    // CopyLogTest.UnreachableSourceIsProvenOnceNotOncePerPass). A target
    // still has to be allowed to not exist yet - SyncDir/SyncTree create it.
    if (!must_exist)
        return ::drive_exists(drive_name);
    std::error_code ec;
    return fs::exists(without_volume_spec(pathname), ec);
#endif
}

bool CheckPath::ssh_path_reachable(const wstring& pathname, bool must_exist) const
{
    log_debug(L"ssh_path_reachable: " + pathname);
    wstring remote_path;
    auto account = ssh_account_of_path(pathname, remote_path);
    if (!account.has_value()) {
        // No account for this prefix: the path cannot be resolved, so nothing is reachable.
        log_error(L"SSH-Account nicht gefunden fuer: " + pathname);
        return false;
    }
    try {
        // The constructor connects, verifies the host key and logs in; it throws
        // if any of that fails. This is the SSH counterpart of "drive is mounted".
        SshFileInspector inspector(account.value());
        if (!must_exist)
            return true;
        // sftp_stat answers for files and directories alike. exists=false means
        // "connected and definitely absent": for a source that is a reason to
        // pause, not to touch the target, or a deleted source directory with
        // "delete in target" enabled would empty the target.
        const RemoteFileInfo info = inspector.inspect_file(wstring_to_string(remote_path));
        if (!info.exists)
            log_info(L"SSH-Pfad existiert nicht: " + pathname);
        return info.exists;
    }
    catch (const std::exception& e) {
        log_info(L"SSH-Pfad nicht erreichbar: " + pathname + L" - " + string_to_wstring(e.what()));
        return false;
    }
    catch (...) {
        log_info(L"SSH-Pfad nicht erreichbar: " + pathname);
        return false;
    }
}

bool CheckPath::side_reachable(const wstring& pathname, bool must_exist) const
{
    wstring remote_path;
    if (ssh_account_of_path(pathname, remote_path).has_value())
        return ssh_path_reachable(pathname, must_exist);
    return path_exists(pathname, must_exist);
}

bool CheckPath::expired(const DriveCheckCacheEntry& entry)
{
    const auto age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry.timestamp).count();
    return age >= entry.time_interval;
}

bool CheckPath::both_sides_reachable(const std::wstring& source, const std::wstring& dest) const
{
    // The source must exist because it is read from. For the target it is enough
    // that the remote side answers: SyncDir and SyncTree create a missing target
    // directory themselves, and a target yet to be created must not block the sync.
    bool result = true;
    if (!side_reachable(source, true)) {
        log_info(L"Quellverzeichnis nicht erreichbar: " + source);
        result = false;
    }
    if (!side_reachable(dest, false)) {
        log_info(L"Zielverzeichnis nicht erreichbar: " + dest);
        result = false;
    }
    return result;
}

// The path itself tells which side is behind SSH, so ssh is not needed.
bool CheckPath::operator()(const std::wstring& source, const std::wstring& dest, [[maybe_unused]] bool ssh)
{
    const std::wstring cache_key = source + L"|" + dest;
    std::lock_guard<std::mutex> lock(drive_check_cache_mutex_);
    const auto cached = drive_check_cache_.find(cache_key);
    const bool have_previous = cached != drive_check_cache_.end();
    if (have_previous && !expired(cached->second)) {
        log_debug(L"CheckPath: cached result for " + cache_key);
        return cached->second.result;
    }
    const bool result = both_sides_reachable(source, dest);
    // The interval only grows when a re-check confirms the same state; a state
    // change (drive appears or disappears) resets to frequent checks instead of
    // keeping the old backoff of up to an hour.
    const uint16_t previous_interval = have_previous ? cached->second.time_interval : BASE_INTERVAL;
    const bool repeats = have_previous && cached->second.result == result;
    drive_check_cache_[cache_key] = {result, next_check_interval(previous_interval, repeats, result), std::chrono::steady_clock::now()};
    return result;
}

wstring root_name(const wstring& path)
{
    log_debug(L"root_name: " + path);
    if (path_contains_volume_name(path)) {
        const auto open_parenthesis_pos = path.find(L'(');
        const auto close_parenthesis_pos = path.find(L')');
        if (close_parenthesis_pos == wstring::npos)
            throw std::runtime_error("Ungültiger Pfad: " + wstring_to_string(path));
        return path.substr(open_parenthesis_pos + 1, close_parenthesis_pos - open_parenthesis_pos - 1);
    }
    const wstring retval = fs::path(path).root_name().wstring();
    log_debug(L"root_name returns: " + retval);
    return retval;
}

std::wstring without_volume_spec(const std::wstring& path)
{
    log_debug(L"without_volume_spec: " + path);
    // Strip only a real volume spec ("Label (C:)rest", paren before colon).
    // Merely containing parentheses ("/home/x/Docs (2023)/f") must not strip,
    // or the path degenerates to a wrong/empty one and the item never syncs.
    if (!path_contains_volume_name(path))
        return path;
    const auto close_parenthesis_pos = path.find(L')');
    if (close_parenthesis_pos == wstring::npos)
        return path;
    return root_name(path) + path.substr(close_parenthesis_pos + 1);
}

} // namespace AFS_Core
