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
#include "SyncFileSsh.h"
#include "SyncFile.h"
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

Synchronize::Synchronize(SyncType sync_type, SyncControler* sc, SyncRole role)
  : sync_type_(sync_type),
    sync_controler_(sc),
    role_(role),
    arbiter_(role)
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

Synchronize* Synchronize::create(SyncType sync_type, SyncControler* sc, SyncRole role)
{
    log_debug(L"Synchronize::create()");
    Synchronize* retval = new Synchronize(sync_type, sc, role);  // [Synchronize.md#constructor-clears-stop]
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
    if (this_ptr)
        this_ptr->messages_.push(SyncMessage(m, file, this_ptr->count_files_, sourcesize));
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
    sout << L"Synchronize::pauseElapsed() for " << si.name() << L": " << std::boolalpha  << retval;
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

void Synchronize::sync_item(SyncItem& i)
{
    log_debug(L"Synchronize::sync_item: ", i.name());
    if (!i.enabled())
        return;
    log_debug(L"Synchronize::operator()() Start of ", i.name());
    if (!pause_elapsed(i))
        return;
    if (!i.backup_proof() || sync_type_ == SyncType::CountFiles  // [Synchronize.md#no-proof-for-counting-passes]
        || Config::instance().settings().backup_proof_path().empty())
        run_sync_item(i);
    else
        prove_sync_item(i);
}

void Synchronize::report_unreachable(const SyncItem& i)
{
    bool& reachable = item_reachable_.try_emplace(i.name(), true).first->second;
    if (!std::exchange(reachable, false))
        return;
    const wstring message = translation::translate("source_or_target_not_reachable") + i.source() + L" -> " + i.target();
    log_error(message);  // [Synchronize.md#item_reachable_]
    report_sync_error(message);
}

void Synchronize::prove_sync_item(SyncItem& i)
{
    SyncReport report;
    const std::size_t files_before = count_files_;
    {
        SyncReportGuard guard(&report);
        run_sync_item(i);
    }
    const std::size_t files = count_files_ - files_before;
    if (files == 0 && report.empty())
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
        if (i.sync_item_type() == SyncItemType::Dir) {
            SyncDir sd(i, count_files_, sync_type_==SyncType::CountFiles);
            sd();
        }
        else if (i.sync_item_type() == SyncItemType::File) {
            std::error_code ec;
            const bool is_target_dir = i.ssh_target() ? is_ssh_target_dir(i.target()) : fs::is_directory(without_volume_spec(i.target()), ec);
            SyncFile(i, count_files_, sync_type_ == SyncType::CountFiles, is_target_dir)();
        }
        else if (i.sync_item_type() == SyncItemType::Tree) {
            SyncTree sync(i, count_files_, sync_type_ == SyncType::CountFiles);
            sync();
        }
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
    if (sync_type_ != SyncType::CountFiles)
        Config::instance().refresh_last_sync(i.name());  // [Synchronize.md#snapshot-vs-config]
}

void Synchronize::operator()()
{
    log_debug(L"Synchronize::operator()()");
    this_.store(this);
    try {
        do {
#ifdef _TEST
            ++loop_iterations_;
#endif
            if (!arbiter_.acquire(stop_request_))
                break;
            for (auto& i: Config::instance().syncitems()) {
                if (stop_request_.load())
                    break;
                sync_item(i);
            }
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
        sync_controler_->set_sync_finished();
    }
#endif
    log_debug(L"End of Synchronize::operator()()");
}

} // namespace AFS_Core
