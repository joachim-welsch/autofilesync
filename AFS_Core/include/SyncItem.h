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
#include <set>
#include <vector>
#include <chrono>
#include <memory>
#include "SshAccount.h"

namespace AFS_Core {

enum class SSHPATH {none, source, dest, both};

class SyncFlags {
    bool    del_aftercopy_ = false;
    bool    two_way_sync_ = false;
    bool    copy_when_target_newer_ = false;
    bool    father_son_principle_ = false;
    bool    delete_in_target_ = false;
    bool    backup_proof_ = false;
    bool    sync_empty_files_ = false;  // [SyncItem.md#sync_empty_files_]
    bool    enabled_ = true;
    uint16_t sync_cycle_ = 60;
public:
    SyncFlags() = default;
    SyncFlags(bool del_aftercopy, bool tws, bool cwtn, bool fsp, bool enabled);
    bool del_aftercopy() const;
    void del_aftercopy(bool v);
    bool two_way_sync() const;
    void two_way_sync(bool v);
    bool copy_when_target_newer() const;
    void copy_when_target_newer(bool v);
    bool father_son_principle() const;
    void father_son_principle(bool v);
    bool delete_in_target() const;
    void delete_in_target(bool v);
    bool backup_proof() const;
    void backup_proof(bool v);
    bool sync_empty_files() const;
    void sync_empty_files(bool v);
    bool enabled() const;
    void enabled(bool v);
};

enum class CompareItem { size, time, content };
constexpr uint16_t DEFAULT_TIME_TOLERANCE_SECONDS = 2;  // [SyncItem.md#time_tolerance_seconds_]

class CompareItems: std::set<CompareItem>{
    uint16_t time_tolerance_seconds_ = DEFAULT_TIME_TOLERANCE_SECONDS;  // [SyncItem.md#time_tolerance_seconds_]
public:
    CompareItems();
    void set(const std::wstring& name, bool value);
    bool size() const;
    bool time() const;
    bool content() const;
    void size(bool);
    void time(bool);
    void content(bool);
    uint16_t time_tolerance_seconds() const { return time_tolerance_seconds_; }
    void time_tolerance_seconds(uint16_t seconds) { time_tolerance_seconds_ = seconds; }
};

enum class SyncPathType { Filesystem, Ssh };

class SyncPath {
    SyncPathType type_;
protected:
    explicit SyncPath(SyncPathType type) : type_(type) {}
public:
    virtual ~SyncPath() = default;
    virtual std::wstring path() const = 0;
    virtual std::unique_ptr<SyncPath> clone() const = 0;  // [SyncItem.md#clone]
    SyncPathType type() const { return type_; }
};

class FilesystemPath: public SyncPath {
    std::wstring path_;
public:
    FilesystemPath();
    explicit FilesystemPath(const std::wstring& path);
    std::wstring path() const override;
    std::unique_ptr<SyncPath> clone() const override;
    void set_path(const std::wstring& path);
};

class SshPath: public SyncPath {
    SshAccount account_;
    std::wstring path_;
public:
    SshPath();
    SshPath(const SshAccount& account, const std::wstring& path);
    std::wstring path() const override;
    std::unique_ptr<SyncPath> clone() const override;
    void set_path(const std::wstring& path);
    const SshAccount& account() const;
    void set_account(const SshAccount& account);
};

class Exclusions: public std::vector<std::wstring> {
public:
};

bool is_excluded(const std::wstring& name, const Exclusions& exclusions);  // [SyncItem.md#is_excluded]

enum class SyncItemType {
    Undefined,
    File,
    Dir,
    Tree
};

std::wstring sync_item_type_to_str(SyncItemType);       // translated, for display
std::wstring sync_item_type_key(SyncItemType);        // untranslated, for the configuration file
SyncItemType str_to_sync_item_type(const std::wstring&);

class SyncItem {
    using TimePoint = std::chrono::steady_clock::time_point;
    using SyncPathPtr = std::unique_ptr<SyncPath>;
    std::wstring    name_;  // key field
    SyncItemType    type_ = SyncItemType::Undefined;
    SyncPathPtr     source_;
    SyncPathPtr     target_;
    std::wstring    wildcard_;
    SyncFlags       flags_;
    CompareItems    compare_items_;
    Exclusions      exclusions_;
    uint16_t        sync_cycle_ = 60;
    TimePoint       last_sync_ = (TimePoint::min)(); // not persisted; min makes the first sync run immediately
    SshPath* ssh_path(const std::unique_ptr<SyncPath>& path) const;
public:
    SyncItem() = default;
    SyncItem(const SyncItem& other);
    SyncItem& operator=(const SyncItem& other);
    SyncItem(SyncItem&&) = default;
    SyncItem& operator=(SyncItem&&) = default;
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w);
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w, const Exclusions& e,
             bool del_aftercopy, bool tws, bool cwtn, bool fsp, bool enabled, const CompareItems&);
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w, const Exclusions& e,
             const SyncFlags& sync_flags, const CompareItems&);
    bool operator==(const SyncItem& o) const;
    std::wstring name() const;
    void name(const std::wstring& n);
    std::wstring source() const;
    bool ssh_source() const;
    void source(const std::wstring& s);
    std::wstring target() const;
    bool ssh_target() const;
    void target(const std::wstring& t);
    const SyncPath* source_path() const;
    const SyncPath* target_path() const;
    void set_source_path(SyncPathPtr path);
    void set_target_path(SyncPathPtr path);
    std::wstring wildcard() const;
    void wildcard(const std::wstring& w);
    bool del_aftercopy() const;
    void del_aftercopy(bool d);
    bool two_way_sync() const;
    void two_way_sync(bool t);
    bool copy_when_target_newer() const;
    void copy_when_target_newer(bool c);
    bool father_son_principle() const;
    void father_son_principle(bool f);
    bool delete_in_target() const;
    void delete_in_target(bool d);
    bool backup_proof() const;
    void backup_proof(bool b);
    bool sync_empty_files() const;
    void sync_empty_files(bool s);
    bool enabled() const;
    void enabled(bool e);
    SyncItemType sync_item_type() const;
    std::wstring item_type_str() const;
    void sync_item_type(SyncItemType t);
    SyncFlags flags() const;
    void flags(const SyncFlags& sf);
    CompareItems compare_items() const;
    void compare_items(const CompareItems& ci);
    void add_exclusion(const std::wstring& excl);
    void exclusions(const Exclusions& exc);
    Exclusions exclusions() const;
    void sync_cycle(uint16_t p);
    uint16_t sync_cycle() const;
    void set_last_sync();
    TimePoint last_sync() const;
    SSHPATH ssh_path_type() const;
    bool rename_ssh_account_reference(const std::string& old_name, const SshAccount& new_account);  // [SyncItem.md#rename_ssh_account_reference]
    bool references_ssh_account(const std::string& name) const;
};

using SyncItems=std::vector<SyncItem>;

inline bool SyncFlags::del_aftercopy() const { return del_aftercopy_; }
inline void SyncFlags::del_aftercopy(bool v) { del_aftercopy_ = v; }
inline bool SyncFlags::two_way_sync() const { return two_way_sync_; }
inline void SyncFlags::two_way_sync(bool v) { two_way_sync_ = v; }
inline bool SyncFlags::copy_when_target_newer() const { return copy_when_target_newer_; }
inline void SyncFlags::copy_when_target_newer(bool v) { copy_when_target_newer_ = v; }
inline bool SyncFlags::father_son_principle() const { return father_son_principle_; }
inline void SyncFlags::father_son_principle(bool v) { father_son_principle_ = v; }
inline bool SyncFlags::delete_in_target() const { return delete_in_target_; }
inline void SyncFlags::delete_in_target(bool v) { delete_in_target_ = v; }
inline bool SyncFlags::backup_proof() const { return backup_proof_; }
inline void SyncFlags::backup_proof(bool v) { backup_proof_ = v; }
inline bool SyncFlags::sync_empty_files() const { return sync_empty_files_; }
inline void SyncFlags::sync_empty_files(bool v) { sync_empty_files_ = v; }
inline bool SyncFlags::enabled() const { return enabled_; }
inline void SyncFlags::enabled(bool v) { enabled_ = v; }

inline bool SyncItem::operator==(const SyncItem& o) const { return o.name_ == name_; }
inline std::wstring SyncItem::name() const { return name_; }
inline void SyncItem::name(const std::wstring& n) { name_ = n; }
inline std::wstring SyncItem::source() const { return source_ ? source_->path() : L""; }
inline bool SyncItem::ssh_source() const { return source_ && source_->type() == SyncPathType::Ssh; }
inline std::wstring SyncItem::target() const { return target_ ? target_->path() : L""; }
inline bool SyncItem::ssh_target() const { return target_ && target_->type() == SyncPathType::Ssh; }
inline const SyncPath* SyncItem::source_path() const { return source_.get(); }
inline const SyncPath* SyncItem::target_path() const { return target_.get(); }
inline void SyncItem::set_source_path(SyncPathPtr path) { source_ = std::move(path); }
inline void SyncItem::set_target_path(SyncPathPtr path) { target_ = std::move(path); }
inline std::wstring SyncItem::wildcard() const { return wildcard_; }
inline void SyncItem::wildcard(const std::wstring& w) { wildcard_ = w; }
inline bool SyncItem::del_aftercopy() const { return flags_.del_aftercopy(); }
inline void SyncItem::del_aftercopy(bool d) { flags_.del_aftercopy(d); }
inline bool SyncItem::two_way_sync() const { return flags_.two_way_sync(); }
inline void SyncItem::two_way_sync(bool t) { flags_.two_way_sync(t); }
inline bool SyncItem::copy_when_target_newer() const { return flags_.copy_when_target_newer(); }
inline void SyncItem::copy_when_target_newer(bool c) { flags_.copy_when_target_newer(c); }
inline bool SyncItem::father_son_principle() const { return flags_.father_son_principle(); }
inline void SyncItem::father_son_principle(bool f) { flags_.father_son_principle(f); }
inline bool SyncItem::delete_in_target() const { return flags_.delete_in_target(); }
inline void SyncItem::delete_in_target(bool d) { flags_.delete_in_target(d); }
inline bool SyncItem::backup_proof() const { return flags_.backup_proof(); }
inline void SyncItem::backup_proof(bool b) { flags_.backup_proof(b); }
inline bool SyncItem::sync_empty_files() const { return flags_.sync_empty_files(); }
inline void SyncItem::sync_empty_files(bool s) { flags_.sync_empty_files(s); }
inline bool SyncItem::enabled() const { return flags_.enabled(); }
inline void SyncItem::enabled(bool e) { flags_.enabled(e); }
inline std::wstring SyncItem::item_type_str() const { return sync_item_type_to_str(type_); }
inline void SyncItem::sync_item_type(SyncItemType t) { type_ = t; }
inline SyncFlags SyncItem::flags() const { return flags_; }
inline void SyncItem::flags(const SyncFlags& sf) { flags_ = sf; }
inline CompareItems SyncItem::compare_items() const { return compare_items_; }
inline void SyncItem::compare_items(const CompareItems& ci) { compare_items_ = ci; }
inline void SyncItem::add_exclusion(const std::wstring& excl) { exclusions_.push_back(excl); }
inline void SyncItem::exclusions(const Exclusions& exc) { exclusions_ = exc; }
inline Exclusions SyncItem::exclusions() const { return exclusions_; }
inline void SyncItem::sync_cycle(uint16_t p) { sync_cycle_ = p; }
inline uint16_t SyncItem::sync_cycle() const { return sync_cycle_; }
inline SyncItem::TimePoint SyncItem::last_sync() const { return last_sync_; }

} // namespace AFS_Core
