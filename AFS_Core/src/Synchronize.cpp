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
#include "pch.h"
#include "Synchronize.h"
#include "DeviceLink.h"
#ifdef _WXWIDGETS
    #ifdef WIN32
        #include <Windows.h>
        #include <tchar.h>
        #include <wincon.h>
    #endif
#endif
#include <string>
#include <iostream>
#include <fstream>
#include "Util.h"
#include "Config.h"
#include <sstream>
#include <filesystem>
#include "Global.h"
#include "SyncItem.h"
#include "SftpConnection.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "SyncState.h"
#include "SyncTree.h"
#include "SyncDir.h"
#include "SyncControler.h"
#include "MountedDrives.h"
#include "CopyLog.h"
#include "Translation.h"
#include <FilesystemUtil.h>
#include <utility>
#include <mutex>

namespace AFS_Core {

using namespace util;
namespace fs = std::filesystem;
using std::wstring;

std::atomic<Synchronize*> Synchronize::this_ = nullptr;
std::atomic_bool Synchronize::stop_ = true;

bool Synchronize::has_instance()
{
    return this_.load() != nullptr;
}

bool Synchronize::stopping()
{
    return stop_;
}

Synchronize& Synchronize::instance()
{
    log_debug(L"Synchronize::instance()");
    Synchronize* inst = this_.load();
    if (inst == nullptr)
        throw std::runtime_error(translation::translate_utf8("no_sync_active"));
    return *inst;
}

Synchronize::Synchronize(SyncType sync_type, SyncControler* sc, SyncRole role, bool mass_deletion_approved, const std::string& device_filter)
  : sync_type_(sync_type),
    sync_controler_(sc),
    role_(role),
    mass_deletion_approved_(mass_deletion_approved),
    arbiter_(role),
    device_filter_(device_filter)
{
    stop_ = false;
    thread_ = std::thread([this]() { (*this)(); });
}

void Synchronize::destroy()
{
    log_debug(L"Synchronize::destroy()");
    Synchronize* inst = this_.load();
    if (inst != nullptr) {
        delete inst;
        this_.store(nullptr);
    }
}

Synchronize* Synchronize::create(SyncType sync_type, SyncControler* sc, SyncRole role, bool mass_deletion_approved, const std::string& device_filter)
{
    log_debug(L"Synchronize::create()");
    Synchronize* retval = new Synchronize(sync_type, sc, role, mass_deletion_approved, device_filter);  // [Synchronize.md#constructor-clears-stop]
    this_.store(retval);
    return retval;
}

bool Synchronize::has_controler() const
{
    return sync_controler_ != nullptr;
}

Synchronize::~Synchronize()
{
    log_debug(L"Synchronize::~Synchronize()");
    stop_request_ = true;
    stop_ = true;
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
    this_.store(nullptr);
    log_debug(L"Synchronize::~Synchronize() complete");
}

#ifdef _TEST
namespace {
    const std::wstring TEST_LOG_FILE = (std::filesystem::temp_directory_path() / "autofilesync_test.log").wstring();
    std::once_flag remove_test_flag;
    void remove_test_file()
    {
        std::filesystem::remove(TEST_LOG_FILE);
    }
}

void Synchronize::start_logging(LogLevel llevel, const wstring& lfname)
{
    std::call_once(remove_test_flag, remove_test_file);
    Logging::instance().set_log_file_path_name(lfname.empty() ? TEST_LOG_FILE : lfname);
    Logging::instance().set_log_level(llevel);
}
#endif

void Synchronize::stop()
{
   log_debug(L"Synchronize::stop()");
   if (has_instance()) {
       instance().stop_request_ = true;
       instance().cv_.notify_all();
   }
   stop_ = true;
}
#ifdef _TEST
void Synchronize::start()
{
    log_debug(L"Synchronize::start()");
    stop_ = false;
}
#endif
void Synchronize::set_progress_message(const std::wstring& m, const std::wstring& file, int64_t sourcesize)
{
    Synchronize* this_ptr = this_.load();
    if (this_ptr == nullptr)
        return;
    this_ptr->copied_bytes_ = 0;  // [Synchronize.md#copied_bytes_]
    this_ptr->messages_.push(SyncMessage(m, file, this_ptr->count_files_, sourcesize));
}

void Synchronize::report_copied_bytes(uint64_t bytes)
{
    if (Synchronize* this_ptr = this_.load())
        this_ptr->copied_bytes_ = bytes;
}

uint64_t Synchronize::copied_bytes() const
{
    return copied_bytes_;
}

SyncMessage Synchronize::get_message()
{
    SyncMessage retval;
    if (!messages_.empty()) {
        messages_.pop(retval);
        while (!messages_.empty())
            messages_.pop();  //  delete superfluous messages
    }
    return retval;
}

std::size_t Synchronize::count_files() const
{
    return count_files_;
}

bool Synchronize::pause_elapsed(const SyncItem& si)
{
    bool retval = std::chrono::steady_clock::now() >= (si.last_sync() + std::chrono::seconds(si.sync_cycle()));
    std::wstringstream sout;
    sout << L"Synchronize::pause_elapsed() for " << si.name() << L": " << std::boolalpha  << retval;
    log_debug(sout.str());
    if (!retval) {
        log_debug("now: ", std::chrono::steady_clock::now());
        log_debug("last_sync: ", si.last_sync());
    }
    return retval;
}

bool Synchronize::path_exists(const std::wstring& pathname) const
{
    log_debug(L"path_exists()");
    std::error_code ec;
    return fs::exists(pathname, ec);
}

SyncControler& Synchronize::sync_controler() const
{
    return *sync_controler_;
}

bool Synchronize::is_ssh_target_dir(const std::wstring& target)
{
    const auto now = std::chrono::steady_clock::now();
    auto it = ssh_target_dir_cache_.find(target);
    if (it != ssh_target_dir_cache_.end() && now - it->second.checked < std::chrono::seconds(60))
        return it->second.is_dir;
    const bool is_dir = ssh_is_directory(target);
    ssh_target_dir_cache_[target] = { is_dir, now };
    return is_dir;
}

namespace {
    constexpr std::chrono::seconds SETTLE_TIME{2};  // [Synchronize.md#due]
    constexpr std::chrono::seconds WATCHED_SCAN_INTERVAL{600};

    void watch_side(std::vector<WatchedDir>& dirs, const SyncItem& item, const wstring& side, bool ssh)  // [Synchronize.md#watcher_]
    {
        if (ssh)
            return;
        const fs::path path = without_volume_spec(side);
        dirs.push_back({item.name(), item.sync_item_type() == SyncItemType::File ? path.parent_path() : path, item.sync_item_type() == SyncItemType::Tree});
    }
}

void Synchronize::watch_items()  // [Synchronize.md#watcher_]
{
    if (!ChangeWatcher::available())
        return;
    std::vector<WatchedDir> dirs;
    for (const SyncItem& i : Config::instance().syncitems()) {
        if (!i.enabled() || !i.watch_changes())
            continue;
        watch_side(dirs, i, i.source(), i.ssh_source());
        watch_side(dirs, i, i.target(), i.ssh_target());
    }
    if (dirs.empty())
        return;
    watcher_ = std::make_unique<ChangeWatcher>(dirs);
    for (const WatchedDir& dir : dirs)
        if (watcher_->watching(dir.item))
            log_info(translation::translate("watching_changes", {dir.item, dir.dir.wstring()}));
}

bool Synchronize::due(const SyncItem& i)  // [Synchronize.md#due]
{
    if (!watcher_ || !watcher_->watching(i.name()))
        return pause_elapsed(i);
    const auto now = std::chrono::steady_clock::now();
    const std::optional<ChangeWatcher::TimePoint> change = watcher_->last_change(i.name());
    if (change && now - *change >= SETTLE_TIME)
        return true;
    return now >= i.last_sync() + WATCHED_SCAN_INTERVAL;
}

void Synchronize::sync_item(SyncItem& i)
{
    log_debug(L"Synchronize::sync_item: ", i.name());
    if (!i.enabled())
        return;
    log_debug(L"Synchronize::operator()() Start of ", i.name());
    std::string device_id;
    const bool of_device = device_item(i, device_id);  // [Synchronize.md#device-items]
    if (of_device && !DeviceLink::instance().serving(device_id))
        return;
    if (!of_device && sync_type_ != SyncType::Preview && !due(i))  // [Synchronize.md#preview-ignores-the-pause]
        return;
    const auto pass_start = std::chrono::steady_clock::now();
    if (touches_nothing())  // [Synchronize.md#no-proof-for-counting-passes]
        run_sync_item(i);
    else
        report_sync_item(i);
    if (watcher_)
        watcher_->clear(i.name(), pass_start);  // [Synchronize.md#due] a change during the pass leads to one more pass
    if (of_device && !touches_nothing())
        DeviceLink::instance().item_passed(device_id, i.name());
}

bool Synchronize::device_item(const SyncItem& i, std::string& device_id) const  // [Synchronize.md#device-items]
{
    for (const SyncPath* side : {i.source_path(), i.target_path()}) {
        const auto* ssh = dynamic_cast<const SshPath*>(side);
        if (ssh && Config::instance().find_device(ssh->account().name())) {
            device_id = ssh->account().name();
            return true;
        }
    }
    return false;
}

void Synchronize::wake()
{
    if (Synchronize* inst = this_.load())
        inst->cv_.notify_all();
}

void Synchronize::sync_items()
{
    SyncPlanGuard guard(sync_type_ == SyncType::Preview ? &plan_ : nullptr);  // [Synchronize.md#preview]
    [[maybe_unused]] SshSessionScope ssh_sessions;  // [Synchronize.md#ssh-sessions]
    for (auto& i: Config::instance().syncitems()) {
        if (stop_request_.load())
            break;
        std::string device_id;
        if (!device_filter_.empty() && (!device_item(i, device_id) || device_id != device_filter_))
            continue;  // [Synchronize.md#device-filter]
        sync_item(i);
    }
}

void Synchronize::report_unreachable(const SyncItem& i)
{
    bool& reachable = item_reachable_.try_emplace(i.name(), true).first->second;
    if (!std::exchange(reachable, false))
        return;
    const wstring message = translation::translate("source_or_target_not_reachable") + i.source() + L" -> " + i.target();
    if (i.warn_if_unreachable()) {  // [Synchronize.md#unreachable_]
        log_warning(message);
        report_sync_error(message);
    }
    else
        log_info(message);
    unreachable_.push_back({i.name(), message, i.warn_if_unreachable()});
}

void Synchronize::note(LogLevel level, const wstring& text)  // [Synchronize.md#notes]
{
    if (Synchronize* inst = this_.load())
        inst->notes_.push_back({level, util::wstring_to_string(text)});
}

LastRun Synchronize::last_run_of(const SyncItem& i, const SyncReport& report) const
{
    const auto reachable = item_reachable_.find(i.name());
    const auto known = known_files_.find(i.name());
    return {reachable == item_reachable_.end() || reachable->second, report.errors().size(),
        report.errors().empty() ? wstring() : report.errors().front(),
        known == known_files_.end() ? std::nullopt : std::optional(known->second)};
}

namespace {
enum class StateKind { None, PassOnly, Kept };  // [Synchronize.md#sync-state]

StateKind state_kind(const SyncItem& item, SyncType type)
{
    if (type == SyncType::CountFiles)
        return StateKind::None;
    if (item.two_way_sync() || item.track_moves())
        return StateKind::Kept;
    return item.delete_in_target() ? StateKind::PassOnly : StateKind::None;  // [Synchronize.md#pass-only-state]
}

class SyncStateKeeper {  // [Synchronize.md#sync-state]
    const SyncItem& item_;
    const bool persist_;
    std::optional<SyncState> state_;
    fs::path file() const { return sync_state_file_path(sync_state_dir(), item_.name()); }
public:
    SyncStateKeeper(const SyncItem& item, StateKind kind, bool persist)
        : item_(item), persist_(persist && kind == StateKind::Kept)
    {
        if (kind == StateKind::Kept)
            state_ = read_sync_state(file(), item.source(), item.target());
        if (kind != StateKind::None && !state_)
            state_.emplace(item.source(), item.target());
        if (state_)
            state_->pass_only(kind == StateKind::PassOnly);
    }
    ~SyncStateKeeper()
    {
        if (state_ && persist_ && state_->changed())
            write_sync_state(file(), *state_);
    }
    SyncStateKeeper(const SyncStateKeeper&) = delete;
    SyncStateKeeper& operator=(const SyncStateKeeper&) = delete;
    SyncState* state() { return state_ ? &*state_ : nullptr; }
    void complete()  // [Synchronize.md#complete-pass]
    {
        if (state_)
            state_->forget_unseen();
    }
};
}

void Synchronize::report_sync_item(SyncItem& i)
{
    SyncReport report;
    const std::size_t files_before = count_files_;
    {
        SyncReportGuard guard(&report);
        run_sync_item(i);
    }
    const std::size_t files = count_files_ - files_before;
    Config::instance().record_last_run(i.name(), last_run_of(i, report));  // [Synchronize.md#record_last_run]
    if (!i.backup_proof() || Config::instance().settings().backup_proof_path().empty() || (files == 0 && report.empty()))
        return;
    write_copy_log(Config::instance().settings().backup_proof_path(), i, files, report);
}

void Synchronize::run_sync_item(SyncItem& i)
{
    try {
        if (!check_path_(i.source(), i.target(), i.ssh_path_type() != SSHPATH::none)) {
            report_unreachable(i);
            return;
        }
        item_reachable_[i.name()] = true;
        SyncStateKeeper keeper(i, state_kind(i, sync_type_), !touches_nothing());  // [Synchronize.md#sync-state]
        if (keeper.state() != nullptr && mass_deletion_approved_)  // [Synchronize.md#mass-deletion-approved]
            keeper.state()->approve_mass_deletion();
        if (i.sync_item_type() == SyncItemType::Dir) {
            SyncDir sd(i, count_files_, touches_nothing(), keeper.state());
            sd();
        }
        else if (i.sync_item_type() == SyncItemType::File) {
            std::error_code ec;
            const bool is_target_dir = i.ssh_target() ? is_ssh_target_dir(i.target()) : fs::is_directory(without_volume_spec(i.target()), ec);
            SyncFile(i, count_files_, touches_nothing(), is_target_dir, keeper.state())();
        }
        else if (i.sync_item_type() == SyncItemType::Tree) {
            SyncTree sync(i, count_files_, touches_nothing(), keeper.state());
            sync();
        }
        if (!stopping() && !stop_requested())
            keeper.complete();
        if (keeper.state() != nullptr && !keeper.state()->pass_only())
            known_files_[i.name()] = keeper.state()->files().size();
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handle_last_error(ex);
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("sync_item_failed"), i.name());
        log_error(string_to_wstring(ex.what()));
        report_sync_error(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        log_error(translation::translate("sync_item_unknown_error"), i.name());
        report_sync_error(translation::translate("unknown_error"));
        return;
    }
    if (!touches_nothing())
        Config::instance().refresh_last_sync(i.name());  // [Synchronize.md#snapshot-vs-config]
}

void Synchronize::operator()()
{
    log_debug(L"Synchronize::operator()()");
    this_.store(this);
    try {
        if (sync_type_ == SyncType::Continuous)
            watch_items();  // [Synchronize.md#watcher_]
        do {
#ifdef _TEST
            ++loop_iterations_;
#endif
            if (!arbiter_.acquire(stop_request_))
                break;
            sync_items();
            if (role_ == SyncRole::Background)
                arbiter_.release();
            if (sync_type_ == SyncType::Continuous && !stop_) {
                std::unique_lock<std::mutex> lock(cv_mutex_);
                cv_.wait_for(lock, std::chrono::seconds(1), [this]{ return stop_request_.load() || stop_.load(); });
            }
        } while (sync_type_ == SyncType::Continuous && !stop_);
    }
    catch (const std::exception& ex) {
        log_fatal(string_to_wstring(ex.what()));
    }
    catch (...) {
        log_fatal(L"Unknown error caught in Synchronize::operator()()");
    }
    arbiter_.release();

#ifdef _WXWIDGETS
    if (sync_controler_ != nullptr && !sync_controler_->sync_finished()) {
        sync_controler_->count_files(count_files_);
        sync_controler_->plan(std::move(plan_));  // [Synchronize.md#preview]
        sync_controler_->unreachable(std::move(unreachable_));  // [Synchronize.md#unreachable_]
        sync_controler_->notes(std::move(notes_));  // [Synchronize.md#notes]
        sync_controler_->set_sync_finished();
    }
#endif
    finished_ = true;  // [Synchronize.md#finished]
    log_debug(L"End of Synchronize::operator()()");
}

} // namespace AFS_Core
