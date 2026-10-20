/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <set>
#include <map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "Global.h"
#include "Config.h"
#include "ThreadSaveStack.h"
#include "SyncMessage.h"
#include "CheckPath.h"
#include "SyncArbiter.h"
#include "SyncPlan.h"
#include <Logging.h>

namespace AFS_Core {

class SyncReport;

class SyncControler;
class SyncItem;

class Synchronize {
    using MessageStack = ThreadSaveStack<SyncMessage>;

    static std::atomic<Synchronize*>    this_;
    static std::atomic_bool stop_;
    std::atomic<SyncType>   sync_type_;
    std::size_t             count_files_ = 0;
    SyncControler*          sync_controler_ = nullptr;
    std::thread             thread_;
    std::atomic_bool        stop_request_{false};
    MessageStack            messages_;
    std::atomic<uint64_t>   copied_bytes_{0};  // [Synchronize.md#copied_bytes_]
    CheckPath               check_path_;
    std::mutex              cv_mutex_;
    std::condition_variable cv_;
    SyncRole                role_ = SyncRole::Foreground;  // [Synchronize.md#role_]
    bool                    mass_deletion_approved_ = false;  // [Synchronize.md#mass-deletion-approved]
    SyncArbiter             arbiter_{SyncRole::Foreground};
    SyncPlan                plan_;  // [Synchronize.md#preview]
    std::vector<UnreachableItem> unreachable_;  // [Synchronize.md#unreachable_]
    std::vector<LogMessage> notes_;  // [Synchronize.md#notes]
#ifdef _TEST
    std::atomic<int>        loop_iterations_{0}; // counts do-while passes of operator()(), for race regression tests
#endif

    struct SshTargetDirCacheEntry {
        bool is_dir;
        std::chrono::steady_clock::time_point checked;
    };
    std::map<std::wstring, SshTargetDirCacheEntry> ssh_target_dir_cache_;  // [Synchronize.md#ssh_target_dir_cache_]
    std::map<std::wstring, bool> item_reachable_;  // [Synchronize.md#item_reachable_]
    std::map<std::wstring, std::size_t> known_files_;  // [Synchronize.md#sync-state]

    bool has_controler() const;
    bool is_ssh_target_dir(const std::wstring& target);
    bool touches_nothing() const { return sync_type_ == SyncType::CountFiles || sync_type_ == SyncType::Preview; }
    void sync_items();
    void sync_item(SyncItem& i);
    void run_sync_item(SyncItem& i);
    void report_unreachable(const SyncItem& i);
    void report_sync_item(SyncItem& i);  // [Synchronize.md#record_last_run]
    LastRun last_run_of(const SyncItem& i, const SyncReport& report) const;
#ifdef _TEST
public:
#endif
    bool pause_elapsed(const SyncItem&);
    bool path_exists(const std::wstring& pathname) const;
    Synchronize() = default;
    Synchronize(SyncType sync_type, SyncControler* sc = nullptr, SyncRole role = SyncRole::Foreground, bool mass_deletion_approved = false);
    bool stop_requested() const;
    static void stop();  // [Synchronize.md#stop]
protected:
    std::size_t count_files() const;
#ifdef _TEST
public:
    static void start();
#endif
    void operator()();
public:
    static Synchronize* create(SyncType sync_type, SyncControler* sc = nullptr, SyncRole role = SyncRole::Foreground, bool mass_deletion_approved = false);  // [Synchronize.md#mass-deletion-approved]
    static bool has_instance();
    static Synchronize& instance();
    static void destroy();
    virtual ~Synchronize();

    #ifdef _TEST
        void start_logging(LogLevel, const std::wstring& lfname);
        int loop_iterations() const { return loop_iterations_; }
        const std::vector<UnreachableItem>& unreachable() const { return unreachable_; }  // the hand-off to the controller is wx-only, tests read it here
        const SyncPlan& plan() const { return plan_; }  // same reason
        const std::vector<LogMessage>& notes() const { return notes_; }
    #endif
    static void set_progress_message(const std::wstring&, const std::wstring& file, int64_t sourcesize);
    static void report_copied_bytes(uint64_t bytes);  // [Synchronize.md#copied_bytes_]
    static void note(LogLevel level, const std::wstring& text);  // [Synchronize.md#notes]
    SyncMessage get_message();
    uint64_t copied_bytes() const;
    SyncControler& sync_controler() const;
    static bool stopping();
    friend class SyncControler;
};

inline
bool Synchronize::stop_requested() const
{
    return stop_request_.load();
}

} // namespace AFS_Core
