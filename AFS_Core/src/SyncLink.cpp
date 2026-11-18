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
#include "Logging.h"
#include "SyncFile.h"
#include "SyncFileSsh.h"
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

fs::path textual(const wstring& path, bool ssh)  // [SyncLink.md#places]
{
    return ssh ? fs::path(get_remote_path(path)) : fs::path(path);
}

bool taken(const wstring& path, bool ssh)
{
    if (ssh) {
        const std::optional<SshLink> info = ssh_link_info(path);
        return info && info->present;  // [SyncLink.md#places]
    }
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}

wstring free_name(const wstring& base, int first_number, bool ssh)  // [SyncLink.md#park]
{
    for (int number = first_number;; ++number) {
        const wstring candidate = number == 0 ? base : base + std::to_wstring(number);
        if (!taken(candidate, ssh))
            return candidate;
    }
}

FileInfoResult renamed(const wstring& from, const wstring& to, bool ssh)
{
    if (!ssh)
        return fsu::rename_refusal(from, to);
    return ssh_rename(from, to) ? FileInfoResult::OK : FileInfoResult::ErrorWriteFileInfo;
}

void discard(const wstring& path, bool ssh)
{
    if (!ssh)
        fsu::remove_file(path);
    else if (taken(path, ssh))
        ssh_remove_file(path);
}

FileInfoResult link_made(const fs::path& text, const wstring& at, bool to_directory, bool ssh)
{
    return ssh ? ssh_create_link(text.generic_wstring(), at) : symlink_util::create_link_refusal(text, at, to_directory);
}

fs::path stored_text(const wstring& at, bool ssh)
{
    return ssh ? fs::path(ssh_link_info(at).value_or(SshLink{}).text) : symlink_util::link_info(at).text;
}

FileInfoResult created_and_read_back(const fs::path& text, const wstring& at, bool to_directory, bool ssh)  // [SyncLink.md#create]
{
    discard(at, ssh);
    const FileInfoResult refusal = link_made(text, at, to_directory, ssh);
    if (refusal != FileInfoResult::OK || stored_text(at, ssh) == text)
        return refusal;
    discard(at, ssh);
    return FileInfoResult::ErrorWriteFileInfo;
}

void report(const wstring& message, FileInfoResult refusal = FileInfoResult::ErrorWriteFileInfo)
{
    log_error(message);
    report_sync_refusal(message, refusal);
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
    , source_{without_volume_spec(si.source()), state_root_, si.ssh_source()}
    , target_{without_volume_spec(si.target()), target_root_, si.ssh_target()}
{
}

std::optional<SyncLink::Side> SyncLink::unasked(const Place& place, FileInfoResult refusal) const  // [SyncLink.md#unasked]
{
    if (!only_count_)
        report(fsu::make_error_message(refusal) + L": " + place.path, refusal);
    return std::nullopt;
}

std::optional<SyncLink::Side> SyncLink::side(const Place& place) const  // [SyncLink.md#places]
{
    const auto portable = [&](const fs::path& text) { return symlink_util::portable_link_text(textual(place.path, place.ssh), text, textual(place.root, place.ssh)); };
    if (place.ssh) {
        const std::optional<SshLink> info = ssh_link_info(place.path);
        if (!info)
            return unasked(place, FileInfoResult::ErrorReadFileInfo);
        return Side{info->is_link, info->present, info->is_directory, info->to_directory, info->is_link ? portable(info->text) : fs::path()};
    }
    const symlink_util::LinkInfo info = symlink_util::link_info(place.path);
    if (info.refusal != FileInfoResult::OK)
        return unasked(place, info.refusal);
    std::error_code ec;
    if (!info.is_link)
        return Side{false, taken(place.path, false), fs::is_directory(fs::symlink_status(place.path, ec)), false, {}};
    return Side{true, true, false, info.to_directory, portable(info.text)};
}

std::optional<fs::path> SyncLink::base() const  // [SyncLink.md#decide]
{
    const FileState* known = sync_item_.two_way_sync() && state_ != nullptr ? state_->find(key_) : nullptr;
    return known != nullptr && known->link ? std::optional<fs::path>(*known->link) : std::nullopt;
}

SyncLink::Action SyncLink::decide_two_links(const Side& here, const Side& there) const
{
    if (same(here.text, there.text))
        return completes_moves() ? Action::DeleteInSource : Action::Settle;  // [SyncLink.md#move]
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

bool SyncLink::blocked_by_directory(const Side& side, const Place& place) const  // [SyncLink.md#blocked-by-directory]
{
    if (!side.directory)
        return false;
    report(translation::translate("link_blocked_by_directory") + place.path);
    return true;
}

void SyncLink::settle(const fs::path& text)
{
    if (state_ != nullptr && !only_count_)
        state_->record(key_, FileState{0, {}, {}, text.generic_wstring()});
}

std::optional<wstring> SyncLink::park(const Place& at) const  // [SyncLink.md#park]
{
    if (!taken(at.path, at.ssh))
        return wstring();
    const wstring parked = sync_item_.father_son_principle() ? free_name(at.path + FATHER_SON_SUFFIX, 1, at.ssh) : free_name(at.path + PARKED_SUFFIX, 0, at.ssh);
    const FileInfoResult refusal = renamed(at.path, parked, at.ssh);
    if (refusal == FileInfoResult::OK)
        return parked;
    report(fsu::make_error_message(refusal) + L": " + at.path, refusal);
    return std::nullopt;
}

bool SyncLink::put_in_place(const wstring& prepared, const Place& at) const
{
    const std::optional<wstring> parked = park(at);
    if (!parked)
        return false;
    const FileInfoResult refusal = renamed(prepared, at.path, at.ssh);
    if (refusal != FileInfoResult::OK) {
        report(translation::translate("link_create_failed") + at.path + L" (" + fsu::make_error_message(refusal) + L")", refusal);
        if (!parked->empty())
            renamed(*parked, at.path, at.ssh);
        return false;
    }
    if (!parked->empty() && !sync_item_.father_son_principle())
        discard(*parked, at.ssh);
    return true;
}

void SyncLink::refused(const Place& at, FileInfoResult refusal)
{
    if (refusal == FileInfoResult::LinksNotSupported)
        tally_.unsupported.add(at.path);
    else
        report(translation::translate("link_create_failed") + at.path + L" (" + fsu::make_error_message(refusal) + L")", refusal);
}

bool SyncLink::create(const fs::path& text, bool to_directory, const Place& at)  // [SyncLink.md#create]
{
    const wstring prepared = at.path + PREPARED_SUFFIX;
    const FileInfoResult refusal = created_and_read_back(text, prepared, to_directory, at.ssh);
    if (refusal != FileInfoResult::OK) {
        refused(at, refusal);
        return false;
    }
    if (put_in_place(prepared, at))
        return true;
    discard(prepared, at.ssh);
    return false;
}

void SyncLink::plan(const Place& destination) const
{
    const bool to_target = &destination == &target_;
    const PlannedAction action = !to_target ? PlannedAction::CopyToSource : sync_item_.del_aftercopy() && !source_kept_ ? PlannedAction::MoveToTarget : PlannedAction::CopyToTarget;
    plan_step({sync_item_.name(), action, source_.path, target_.path, 0, true});
}

void SyncLink::carried(const Side& from, const Place& origin, const Place& destination)
{
    ++copy_count_;
    log_info(translation::translate("link_created"), origin.path, L"->", destination.path);
    if (symlink_util::leaves_root(textual(origin.path, origin.ssh), from.text, textual(origin.root, origin.ssh)))
        tally_.outside.add(origin.path);
    else if (!destination.ssh)
        tally_.created.push_back(destination.path);
    settle(from.text);
    if (&destination == &target_ && moves_ && deleted(origin.path, false, "source_delete_failed") && state_ != nullptr)  // [SyncLink.md#move]
        state_->forget(key_);
}

void SyncLink::carry(const Side& from, const Place& origin, const Side& to, const Place& destination)  // [SyncLink.md#create]
{
    if (blocked_by_directory(to, destination))
        return;
    if (only_count_) {
        ++copy_count_;
        return plan(destination);
    }
    if (create(from.text, from.to_directory, destination))
        carried(from, origin, destination);
}

void SyncLink::remove(bool in_target)  // [SyncLink.md#remove]
{
    const Place& doomed = in_target ? target_ : source_;
    if (state_ != nullptr && sync_item_.sync_item_type() != SyncItemType::File) {
        state_->defer({key_, source_.path, target_.path, in_target, 0, true});
        return;
    }
    if (only_count_) {
        ++copy_count_;
        plan_step({sync_item_.name(), in_target ? PlannedAction::DeleteInTarget : PlannedAction::DeleteInSource, source_.path, target_.path, 0, true});
        return;
    }
    if (deleted(doomed.path, doomed.ssh, in_target ? "target_delete_failed" : "source_delete_failed"))
        log_info(translation::translate("link_removed"), doomed.path);
}

void SyncLink::resolve_conflict(const Side& here, const Side& there)  // [SyncLink.md#conflict]
{
    if (!sync_item_.conflict_copies()) {
        const wstring message = translation::translate("both_modified_not_copied") + L" " + source_.path + L" <-> " + target_.path;
        if (first_conflict(target_.path))
            log_error(message);
        report_sync_error(message);
        return;
    }
    if (blocked_by_directory(there, target_))
        return;
    if (only_count_) {
        ++copy_count_;
        return plan_step({sync_item_.name(), PlannedAction::ConflictCopyToTarget, source_.path, target_.path, 0, true});
    }
    const wstring copy = conflict_copy_name(target_.path, target_.ssh);
    const FileInfoResult refusal = renamed(target_.path, copy, target_.ssh);
    if (refusal != FileInfoResult::OK)
        return report(fsu::make_error_message(refusal) + L": " + target_.path, refusal);
    const wstring made = translation::translate("conflict_copy_made", {target_.path, copy.substr(copy.find_last_of(L"/\\") + 1)});
    log_warning(made);
    Synchronize::note(LogLevel::Warning, made);  // [Synchronize.md#notes]
    carry(here, source_, Side{}, target_);
}

void SyncLink::carry_out(Action action, const Side& here, const Side& there)
{
    switch (action) {
    case Action::Leave: return;
    case Action::Settle: return settle(here.text);
    case Action::ToTarget: return carry(here, source_, there, target_);
    case Action::ToSource: return carry(there, target_, here, source_);
    case Action::DeleteInTarget: return remove(true);
    case Action::DeleteInSource: return remove(false);
    case Action::Conflict: return resolve_conflict(here, there);
    }
}

void SyncLink::operator()()
{
    if ((source_.ssh && !ssh_keeps_links(source_.path)) || (target_.ssh && !ssh_keeps_links(target_.path))) {  // [SyncLink.md#no-links-there]
        tally_.unsupported.add(source_.path);
        return;
    }
    key_ = state_key(source_.path);
    if (state_ != nullptr)
        state_->seen(key_);
    const std::optional<Side> here = side(source_), there = side(target_);
    if (here && there)  // [SyncLink.md#places]
        carry_out(decide(*here, *there), *here, *there);
}

} // namespace AFS_Core
