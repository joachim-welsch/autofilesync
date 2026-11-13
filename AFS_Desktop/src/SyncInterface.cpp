/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "SyncInterface.h"
#include "Util.h"
#include "SyncControler.h"
#include "SyncItem.h"
#include "ProgressDialog.h"
#include "PreviewDialog.h"
#include "Synchronize.h"
#include "Menu.h"
#include "Config.h"
#include "Logging.h"
#include "id.h"
#include "utilwx.h"
#include "Translation.h"
#include <algorithm>
#include <optional>
#include <vector>

using namespace AFS_Core;

using utilwx::tul;

SyncInterface::SyncInterface(wxWindow& parent_window, Menu& menu)
    : parent_window_(parent_window)
    , menu_(menu)
    , sync_controler_(SyncControler::instance())
{
}

SyncInterface::~SyncInterface()
{
    if (progress_dialog_ != nullptr) {
        progress_dialog_->can_be_closed(true);
        progress_dialog_->Destroy();
        progress_dialog_ = nullptr;
    }
}

void SyncInterface::hide_progress_dialog()  // [SyncInterface.md#reuse-progress-dialog]
{
    if (progress_dialog_ == nullptr)
        return;
    progress_dialog_->can_be_closed(true);
    progress_dialog_->Hide();  // kept alive and reused: destroying and recreating a top-level window makes the next one fail to reach the screen on macOS
}

bool SyncInterface::silent_mode() const
{
    return console_mode_ || Config::instance().settings().start_minimized();
}

void SyncInterface::set_console_mode()
{
    console_mode_ = true;
}

void SyncInterface::open_progress_dialog(uint32_t n_files)
{
    if (silent_mode())
        return;
    if (progress_dialog_ == nullptr)
        progress_dialog_ = new ProgressDialog(&parent_window_, wxID_ANY, translation::translate("progress_title"), sync_controler_, n_files);
    else
        progress_dialog_->reset(n_files);
    progress_dialog_->Show();
}

namespace {

constexpr std::size_t MAX_LINES_IN_BOX = 15;  // [SyncInterface.md#run_messages_text]

bool wanted(const LogMessage& message, const Settings& settings)
{
    switch (message.level) {
    case LogLevel::Info: return settings.show_infos();
    case LogLevel::Warning: return settings.show_warnings();
    default: return settings.show_errors();
    }
}

wxString level_label(LogLevel level)
{
    switch (level) {
    case LogLevel::Info: return translation::translate("info");
    case LogLevel::Warning: return translation::translate("warning");
    default: return translation::translate("error");
    }
}

wxString run_messages_text(const std::vector<LogMessage>& messages)  // [SyncInterface.md#run_messages_text]
{
    wxString text;
    std::size_t shown = 0;
    for (const LogMessage& message : messages) {
        if (shown++ == MAX_LINES_IN_BOX) {
            text += translation::translate("more_messages", {std::to_wstring(messages.size() - MAX_LINES_IN_BOX)});
            break;
        }
        text += level_label(message.level) + ": " + wxString::FromUTF8(message.text) + "\n";
    }
    return text;
}

LogLevel highest_level(const std::vector<LogMessage>& messages)
{
    LogLevel highest = LogLevel::Info;
    for (const LogMessage& message : messages)
        highest = std::max(highest, message.level);
    return highest;
}

wxString unreachable_text(const std::vector<UnreachableItem>& unreachable)  // [SyncInterface.md#preview-unreachable]
{
    if (unreachable.empty())
        return {};
    std::wstring names;
    for (const UnreachableItem& item : unreachable)
        names += L"\n" + item.name;
    return translation::translate("preview_unreachable", {names});
}

bool any_warns(const std::vector<UnreachableItem>& unreachable)
{
    return std::ranges::any_of(unreachable, [](const UnreachableItem& item) { return item.warn; });
}

void add_notes(std::vector<LogMessage>& messages, const std::vector<LogMessage>& notes)  // [SyncInterface.md#notes]
{
    for (const LogMessage& note : notes)
        if (std::ranges::none_of(messages, [&note](const LogMessage& message) { return message.text == note.text; }))
            messages.insert(messages.begin(), note);
}

void add_unreachable_warnings(std::vector<LogMessage>& messages, const std::vector<UnreachableItem>& unreachable)  // [SyncInterface.md#unreachable-warnings]
{
    for (const UnreachableItem& item : unreachable) {
        const std::string text = util::wstring_to_string(item.message);
        const bool shown = std::ranges::any_of(messages, [&text](const LogMessage& message) { return message.text == text; });
        if (item.warn && !shown)
            messages.insert(messages.begin(), {LogLevel::Warning, text});
    }
}

wxString joined(const wxString& first, const wxString& second)
{
    return first.empty() || second.empty() ? first + second : first + "\n\n" + second;
}

wxString empty_preview_text(const std::vector<UnreachableItem>& unreachable, const std::vector<LogMessage>& messages)  // [SyncInterface.md#preview-unreachable]
{
    const wxString outcome = unreachable.empty() ? wxString(translation::translate("preview_nothing_to_do")) : unreachable_text(unreachable);
    return joined(outcome, run_messages_text(messages));
}

}

std::vector<LogMessage> SyncInterface::take_run_messages()
{
    Logging::instance().collect_messages(false);
    std::vector<LogMessage> messages = Logging::instance().take_collected_messages();
    const Settings& settings = Config::instance().settings();
    std::erase_if(messages, [&settings](const LogMessage& message) { return !wanted(message, settings); });
    return messages;
}

void SyncInterface::show_run_messages()
{
    std::vector<LogMessage> messages = take_run_messages();
    add_unreachable_warnings(messages, sync_controler_.take_unreachable());  // [SyncInterface.md#unreachable-warnings]
    add_notes(messages, sync_controler_.take_notes());  // [SyncInterface.md#notes]
    if (messages.empty() || silent_mode())
        return;
    show_state();  // [SyncInterface.md#show_state]
    const LogLevel highest = highest_level(messages);
    const long icon = highest == LogLevel::Info ? wxICON_INFORMATION : highest == LogLevel::Warning ? wxICON_WARNING : wxICON_ERROR;
    wxMessageBox(run_messages_text(messages), translation::translate("synchronisation") + L": " + level_label(highest).ToStdWstring(), wxOK | icon, &parent_window_);
}

void SyncInterface::sync_once_finished()
{
    log_debug(L"SyncInterface::sync_once_finished()");
    sync_controler_.destroy_synchronize();
    hide_progress_dialog();
    enable_start_commands(true);
    act_sync_type_ = SyncType::None;
    deletions_approved_ = false;
    show_run_messages();
}

void SyncInterface::enable_start_commands(bool enabled)  // [SyncInterface.md#enable_start_commands]
{
    for (const id command : {id::StartSync, id::SyncOnce, id::Preview})
        menu_.sync_menu()->Enable(tul(command), enabled);
}

void SyncInterface::count_files_finished()
{
    log_debug(L"SyncInterface::count_files_finished()");
    counting_cursor_.reset();  // [SyncInterface.md#counting_cursor_]
    sync_controler_.destroy_synchronize();
    count_files_ = 0;
    hide_progress_dialog();
    count_files_ = sync_controler_.count_files();
    if (count_files_ > 0) {
        act_sync_type_ = SyncType::Single;
        start_sync();
    }
    else {
        act_sync_type_ = SyncType::None;
        enable_start_commands(true);  // [SyncInterface.md#terminal-outcome-reenables-menu]
        show_state();  // [SyncInterface.md#show_state]
        wxMessageBox(translation::translate("no_files_to_sync"), translation::translate("info"), wxOK | wxICON_INFORMATION, &parent_window_);
        show_run_messages();
    }
}

void SyncInterface::preview_finished()  // [SyncInterface.md#preview]
{
    log_debug(L"SyncInterface::preview_finished()");
    counting_cursor_.reset();
    sync_controler_.destroy_synchronize();
    act_sync_type_ = SyncType::None;
    enable_start_commands(true);
    show_state();  // [SyncInterface.md#show_state]
    const SyncPlan plan = sync_controler_.take_plan();
    const std::vector<UnreachableItem> unreachable = sync_controler_.take_unreachable();
    std::vector<LogMessage> messages = take_run_messages();
    add_notes(messages, sync_controler_.take_notes());  // [SyncInterface.md#notes]
    if (plan.empty()) {
        const long icon = any_warns(unreachable) ? wxICON_WARNING : wxICON_INFORMATION;
        wxMessageBox(empty_preview_text(unreachable, messages), translation::translate("preview_title"), wxOK | icon, &parent_window_);
        return;
    }
    PreviewDialog dialog(&parent_window_, plan, joined(unreachable_text(unreachable), run_messages_text(messages)));
    if (dialog.ShowModal() == wxID_OK && single_sync()) {
        deletions_approved_ = true;  // [SyncInterface.md#preview-approves-deletions]
        enable_start_commands(false);
    }
}

void SyncInterface::continuous_sync_finished()
{
    sync_controler_.destroy_synchronize();
    act_sync_type_ = SyncType::None;
}

void SyncInterface::on_timer()
{
    log_debug(L"SyncInterface::on_timer()");
    if (!Config::exists() || !Config::instance().ready())
        return;
    if (sync_controler_.sync_finished()) {
        sync_controler_.reset_state();  // [SyncInterface.md#reset-before-the-next-run]
        if (act_sync_type_ == SyncType::CountFiles)
            count_files_finished();
        else if (act_sync_type_ == SyncType::Preview)
            preview_finished();
        else if (act_sync_type_ == SyncType::Single)
            sync_once_finished();
        else if (act_sync_type_ == SyncType::Continuous)
            continuous_sync_finished();
    }
    if (act_sync_type_ == SyncType::Continuous && !Synchronize::has_instance())
        on_sync();
}

bool SyncInterface::start_pass(SyncType pass)
{
    const bool can_sync = Config::instance().ready() && !Config::instance().syncitems().empty() && !Synchronize::has_instance();
    if (!can_sync)
        return false;
    Logging::instance().collect_messages(true);  // [SyncInterface.md#show_run_messages]
    act_sync_type_ = pass;
    deletions_approved_ = false;  // [SyncInterface.md#preview-approves-deletions]
    return start_sync();
}

bool SyncInterface::single_sync()
{
    return start_pass(SyncType::CountFiles);
}

bool SyncInterface::preview()  // [SyncInterface.md#preview]
{
    return start_pass(SyncType::Preview);
}

bool SyncInterface::start_sync()
{
    log_debug(L"SyncInterface::start_sync()");
    if (act_sync_type_ == SyncType::CountFiles || act_sync_type_ == SyncType::Preview)
        counting_cursor_ = std::make_unique<wxBusyCursor>();  // [SyncInterface.md#counting_cursor_]
    else if (act_sync_type_ == SyncType::Single) {
        if (count_files_ == 0) {
            wxMessageBox(translation::translate("no_files_to_sync"), translation::translate("info"), wxOK | wxICON_INFORMATION, &parent_window_);
            return false;
        }
        open_progress_dialog(count_files_);
    }
    if (!sync_controler_.start_sync(act_sync_type_, deletions_approved_ && act_sync_type_ == SyncType::Single)) {  // [SyncInterface.md#preview-approves-deletions]
        counting_cursor_.reset();
        wxMessageBox(translation::translate("sync_start_failed"), translation::translate("error"), wxOK | wxICON_ERROR, &parent_window_);
        return false;
    }
    return true;
}

void SyncInterface::run_console_sync()
{
    log_debug(L"SyncInterface::run_console_sync()");
    if (Config::instance().syncitems().empty() || Synchronize::has_instance())
        return;
    act_sync_type_ = SyncType::Single;  // [SyncInterface.md#console-single-sync]
    if (!sync_controler_.start_sync(act_sync_type_))
        return;
    while (!sync_controler_.sync_finished())  // [SyncInterface.md#poll-for-completion]
        wxMilliSleep(50);
    sync_controler_.destroy_synchronize();
    sync_controler_.reset_state();
}

void SyncInterface::on_sync()
{
    if (Config::instance().syncitems().empty()) {
        if (!silent_mode())
            wxMessageBox(translation::translate("no_sync_items"), translation::translate("attention"), wxOK | wxICON_WARNING, &parent_window_);
        return;
    }
    try {
        if (Synchronize::has_instance()) {
            wxMessageBox(translation::translate("sync_already_running"), translation::translate("error"), wxOK | wxICON_ERROR, &parent_window_);
            return;
        }
        start_sync();
    }
    catch (std::exception& ex) {
        if (!silent_mode())
            wxMessageBox(wxString::FromUTF8(ex.what()), translation::translate("error"), wxOK | wxICON_ERROR, &parent_window_);
        log_error(ex.what());
    }
    catch (...) {
        if (!silent_mode())
            wxMessageBox(translation::translate("sync_failed"), translation::translate("error"), wxOK | wxICON_ERROR, &parent_window_);
        log_error(translation::translate("sync_failed"));
    }
}

void SyncInterface::start_continuous_sync()
{
    log_debug(L"SyncInterface::start_continuous_sync()");
    act_sync_type_ = SyncType::Continuous;
    enable_start_commands(false);
    menu_.sync_menu()->Enable(tul(id::StopSync), true);
}

void SyncInterface::stop_continuous_sync()
{
    log_debug(L"SyncInterface::stop_continuous_sync()");
    enable_start_commands(true);
    menu_.sync_menu()->Enable(tul(id::StopSync), false);
    sync_controler_.stop_sync();
    continuous_sync_finished();  // [SyncInterface.md#stop-leaves-continuous-mode]
    sync_controler_.reset_state();
}

namespace {

struct Waiting {  // [SyncInterface.md#waiting_items]
    bool due = false;
    bool watched = false;
    std::optional<std::chrono::steady_clock::duration> shortest_pause;
};

Waiting waiting_items()  // [SyncInterface.md#waiting_items]
{
    Waiting waiting;
    const auto now = std::chrono::steady_clock::now();
    for (const auto& item : Config::instance().syncitems()) {
        if (!item.enabled())
            continue;
        const auto last_sync = item.last_sync();
        if (item.watch_changes() && last_sync != (std::chrono::steady_clock::time_point::min)()) {
            waiting.watched = true;
            continue;
        }
        const auto cycle = std::chrono::seconds(item.sync_cycle());
        if (last_sync <= now - cycle) {  // last_sync + cycle would overflow for a fresh item's TimePoint::min
            waiting.due = true;
            continue;
        }
        const auto remaining = last_sync + cycle - now;
        if (!waiting.shortest_pause || remaining < *waiting.shortest_pause)
            waiting.shortest_pause = remaining;
    }
    return waiting;
}

}

wxString SyncInterface::state_text() const
{
    switch (act_sync_type_) {
    case SyncType::None: return translation::translate("state_stopped");
    case SyncType::CountFiles: return translation::translate("state_counting");
    case SyncType::Preview: return translation::translate("state_preview");
    case SyncType::Single: return translation::translate("state_single_running");
    case SyncType::Continuous: break;
    }
    const Waiting waiting = waiting_items();
    if (waiting.due)
        return translation::translate("state_running");
    if (waiting.shortest_pause)
        return translation::translate("state_paused", {std::to_wstring(std::chrono::ceil<std::chrono::seconds>(*waiting.shortest_pause).count())});
    if (waiting.watched)
        return translation::translate("state_watching");
    return translation::translate("state_running");
}

void SyncInterface::show_state()  // [SyncInterface.md#show_state]
{
    if (wxFrame* frame = parent_frame())
        frame->SetStatusText(state_text(), 1);
}

void SyncInterface::stop_sync()
{
    sync_controler_.stop_sync();
}
