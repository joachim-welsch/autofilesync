#pragma once
#include <string>
#include <mutex>
#include <map>
#include <chrono>

namespace AFS_Core {

class CheckPath {
    mutable std::mutex  drive_check_cache_mutex_;

    struct DriveCheckCacheEntry {
        bool result;
        uint16_t time_interval;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::map<std::wstring, DriveCheckCacheEntry> drive_check_cache_;
    static bool expired(const DriveCheckCacheEntry& entry);
    bool both_sides_reachable(const std::wstring& source, const std::wstring& dest) const;
    // must_exist only affects the Windows branch: a source must actually be
    // there, but a local target that sync itself still has to create must not
    // be rejected for not existing yet - see side_reachable().
    bool path_exists(const std::wstring& pathname, bool must_exist) const;
    // Reachability of one side regardless of whether it is local or behind SSH;
    // must_exist only applies to SSH (locally path_exists decides).
    bool side_reachable(const std::wstring& pathname, bool must_exist) const;
public:
    CheckPath() = default;
    bool operator()(const std::wstring& source, const std::wstring& dest, bool ssh);
    // Connects to the remote side; must_exist=true additionally requires the path
    // to be present there. Public so the rule is testable without going through
    // the cache of operator()().
    bool ssh_path_reachable(const std::wstring& pathname, bool must_exist) const;
};

// Next cache check interval in seconds. Free-standing so the backoff rule can be
// tested without actually waiting.
uint16_t next_check_interval(uint16_t previous_interval, bool repeats_previous_result, bool reachable);

bool path_contains_volume_name(const std::wstring& path);
std::wstring volume_name(const std::wstring& path);
std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
