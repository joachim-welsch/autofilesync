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
#include "SyncLink.h"
#include "CopyLog.h"
#include "FileDelete.h"
#include "Logging.h"
#include "SyncFile.h"
#include "SyncPlan.h"
#include "Synchronize.h"
#include <mutex>
#include <set>
#include "Translation.h"
#include <SymlinkUtil.h>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using fsu::FileInfoResult;

namespace {
constexpr wchar_t PREPARED_SUFFIX[] = L".afs-link";  // [SyncLink.md#create]
constexpr wchar_t PARKED_SUFFIX[] = L".bak";  // [SyncLink.md#park]

bool taken(const wstring& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}

wstring free_name(const wstring& base, int first_number)  // [SyncLink.md#park]
{
    for (int number = first_number;; ++number) {
        const wstring candidate = number == 0 ? base : base + std::to_wstring(number);
        if (!taken(candidate))
            return candidate;
    }
}

void report(const wstring& message)
{
    log_error(message);
    report_sync_error(message);
}

bool same(const fs::path& a, const fs::path& b)
{
    return a.lexically_normal() == b.lexically_normal();
}

std::mutex conflicts_mutex;
std::set<wstring> logged_conflicts;  // [SyncLink.md#conflict]

bool first_conflict(const wstring& link)
{
    std::lock_guard<std::mutex> lock(conflicts_mutex);
    return logged_conflicts.insert(link).second;
}
}

void LinkTally::Count::add(const wstring& link)
{
    if (links++ == 0)
        first = link;
}

SyncLink::SyncLink(const SyncItem& si, std::size_t& copy_count, bool only_count, LinkTally& tally, SyncState* state,
    const wstring& state_root, const wstring& target_root)
    : SyncBase(si, copy_count, only_count, state, state_root, target_root), tally_(tally)
{
}

SyncLink::Side SyncLink::side(const wstring& path, const wstring& root) const
{
    const symlink_util::LinkInfo info = symlink_util::link_info(path);
    if (!info.is_link)
        return {false, taken(path), false, {}};
    return {true, true, info.to_directory, symlink_util::portable_link_text(path, info.text, root)};
}

std::optional<fs::path> SyncLink::base() const  // [SyncLink.md#decide]
{
    const FileState* known = sync_item_.two_way_sync() && state_ != nullptr ? state_->find(key_) : nullptr;
    return known != nullptr && known->link ? std::optional<fs::path>(*known->link) : std::nullopt;
}

SyncLink::Action SyncLink::decide_two_links(const Side& here, const Side& there) const
{
    if (same(here.text, there.text))
        return Action::Settle;
    if (!sync_item_.two_way_sync())
        return Action::ToTarget;
    const std::optional<fs::path> known = base();
    if (known && same(*known, here.text))
        return Action::ToSource;
    return known && same(*known, there.text) ? Action::ToTarget : Action::Conflict;
}

SyncLink::Action SyncLink::decide_target_only(const Side& there) const
{
    const std::optional<fs::path> known = base();
    if (sync_item_.two_way_sync())
        return known && same(*known, there.text) ? Action::DeleteInTarget : Action::ToSource;
    return sync_item_.delete_in_target() ? Action::DeleteInTarget : Action::Leave;
}

SyncLink::Action SyncLink::decide(const Side& here, const Side& there) const  // [SyncLink.md#decide]
{
    if (here.link && there.link)
        return decide_two_links(here, there);
    if (there.link)
        return here.present ? Action::Leave : decide_target_only(there);
    if (!here.link)
        return Action::Leave;
    if (!sync_item_.two_way_sync())
        return Action::ToTarget;
    if (there.present)
        return Action::Conflict;
    const std::optional<fs::path> known = base();
    return known && same(*known, here.text) ? Action::DeleteInSource : Action::ToTarget;
}

bool SyncLink::blocked_by_directory(const wstring& at) const  // [SyncLink.md#blocked-by-directory]
{
    std::error_code ec;
    if (!fs::is_directory(fs::symlink_status(at, ec)))
        return false;
    report(translation::translate("link_blocked_by_directory") + at);
    return true;
}

void SyncLink::settle(const fs::path& text)
{
    if (state_ != nullptr && !only_count_)
        state_->record(key_, FileState{0, {}, {}, text.wstring()});
}

FileInfoResult SyncLink::create_verified(const fs::path& text, const wstring& at, bool to_directory) const  // [SyncLink.md#create]
{
    fsu::remove_file(at);
    const FileInfoResult refusal = symlink_util::create_link_refusal(text, at, to_directory);
    if (refusal != FileInfoResult::OK || symlink_util::link_info(at).text == text)
        return refusal;
    fsu::remove_file(at);
    return FileInfoResult::ErrorWriteFileInfo;
}

std::optional<wstring> SyncLink::park(const wstring& destination) const  // [SyncLink.md#park]
{
    if (!taken(destination))
        return wstring();
    const wstring parked = sync_item_.father_son_principle() ? free_name(destination + FATHER_SON_SUFFIX, 1) : free_name(destination + PARKED_SUFFIX, 0);
    const FileInfoResult refusal = fsu::rename_refusal(destination, parked);
    if (refusal == FileInfoResult::OK)
        return parked;
    report(fsu::make_error_message(refusal) + L": " + destination);
    return std::nullopt;
}

bool SyncLink::put_in_place(const wstring& prepared, const wstring& destination) const
{
    const std::optional<wstring> parked = park(destination);
    if (!parked)
        return false;
    const FileInfoResult refusal = fsu::rename_refusal(prepared, destination);
    if (refusal != FileInfoResult::OK) {
        report(translation::translate("link_create_failed") + destination + L" (" + fsu::make_error_message(refusal) + L")");
        if (!parked->empty())
            fsu::rename_file(*parked, destination);
        return false;
    }
    if (!parked->empty() && !sync_item_.father_son_principle())
        fsu::remove_file(*parked);
    return true;
}

void SyncLink::refused(const wstring& destination, FileInfoResult refusal)
{
    if (refusal == FileInfoResult::LinksNotSupported)
        tally_.unsupported.add(destination);
    else
        report(translation::translate("link_create_failed") + destination + L" (" + fsu::make_error_message(refusal) + L")");
}

bool SyncLink::create(const fs::path& text, const wstring& destination, bool to_directory)  // [SyncLink.md#create]
{
    const wstring prepared = destination + PREPARED_SUFFIX;
    const FileInfoResult refusal = create_verified(text, prepared, to_directory);
    if (refusal != FileInfoResult::OK) {
        refused(destination, refusal);
        return false;
    }
    if (put_in_place(prepared, destination))
        return true;
    fsu::remove_file(prepared);
    return false;
}

void SyncLink::carry(const Side& from, const wstring& from_path, const wstring& to_path, bool to_target)  // [SyncLink.md#create]
{
    if (blocked_by_directory(to_path))
        return;
    if (only_count_) {
        ++copy_count_;
        const PlannedAction action = !to_target ? PlannedAction::CopyToSource : sync_item_.del_aftercopy() ? PlannedAction::MoveToTarget : PlannedAction::CopyToTarget;
        plan_step({sync_item_.name(), action, to_target ? from_path : to_path, to_target ? to_path : from_path, 0});
        return;
    }
    if (!create(from.text, to_path, from.to_directory))
        return;
    ++copy_count_;
    log_info(translation::translate("link_created"), from_path, L"->", to_path);
    if (symlink_util::leaves_root(from_path, from.text, to_target ? state_root_ : target_root_))
        tally_.outside.add(from_path);
    else
        tally_.created.push_back(to_path);
    settle(from.text);
    if (to_target && sync_item_.del_aftercopy() && delete_file(from_path) && state_ != nullptr)
        state_->forget(key_);
}

void SyncLink::remove(const wstring& source, const wstring& destination, bool in_target)  // [SyncLink.md#remove]
{
    const wstring& doomed = in_target ? destination : source;
    if (state_ != nullptr && sync_item_.sync_item_type() != SyncItemType::File) {
        state_->defer({key_, source, destination, in_target, 0});
        return;
    }
    if (only_count_) {
        ++copy_count_;
        plan_step({sync_item_.name(), in_target ? PlannedAction::DeleteInTarget : PlannedAction::DeleteInSource, source, destination, 0});
        return;
    }
    if (delete_file(doomed))
        log_info(translation::translate("link_removed"), doomed);
    else
        report(translation::translate(in_target ? "target_delete_failed" : "source_delete_failed") + doomed);
}

void SyncLink::resolve_conflict(const wstring& source, const wstring& destination, const Side& here)  // [SyncLink.md#conflict]
{
    if (!sync_item_.conflict_copies()) {
        const wstring message = translation::translate("both_modified_not_copied") + L" " + source + L" <-> " + destination;
        if (first_conflict(destination))
            log_error(message);
        report_sync_error(message);
        return;
    }
    if (only_count_ || blocked_by_directory(destination))
        return;
    const wstring copy = conflict_copy_name(destination, false);
    const FileInfoResult refusal = fsu::rename_refusal(destination, copy);
    if (refusal != FileInfoResult::OK)
        return report(fsu::make_error_message(refusal) + L": " + destination);
    const wstring made = translation::translate("conflict_copy_made", {destination, copy.substr(copy.find_last_of(L"/\\") + 1)});
    log_warning(made);
    Synchronize::note(LogLevel::Warning, made);  // [Synchronize.md#notes]
    carry(here, source, destination, true);
}

void SyncLink::carry_out(Action action, const wstring& source, const wstring& destination, const Side& here, const Side& there)
{
    switch (action) {
    case Action::Leave: return;
    case Action::Settle: return settle(here.text);
    case Action::ToTarget: return carry(here, source, destination, true);
    case Action::ToSource: return carry(there, destination, source, false);
    case Action::DeleteInTarget: return remove(source, destination, true);
    case Action::DeleteInSource: return remove(source, destination, false);
    case Action::Conflict: return resolve_conflict(source, destination, here);
    }
}

void SyncLink::operator()()
{
    const wstring source = without_volume_spec(sync_item_.source());
    const wstring destination = without_volume_spec(sync_item_.target());
    if (sync_item_.ssh_source() || sync_item_.ssh_target()) {  // [SyncLink.md#local-only]
        tally_.unsupported.add(source);
        return;
    }
    key_ = state_key(source);
    if (state_ != nullptr)
        state_->seen(key_);
    const Side here = side(source, state_root_), there = side(destination, target_root_);
    carry_out(decide(here, there), source, destination, here, there);
}

} // namespace AFS_Core
