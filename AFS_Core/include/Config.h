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
#include <vector>
#include <set>
#include <fstream>
#include <optional>
#include <mutex>
#include <filesystem>
#include "SyncItem.h"
#include "Settings.h"
#include "SshAccount.h"
#include <boost/property_tree/json_parser.hpp>

namespace AFS_Core {

using SshAccounts = std::vector<SshAccount>;

class Config {
    Settings            settings_;
    SyncItems           syncitems_;
    mutable std::mutex  syncitems_mutex_;    // [Config.md#syncitems_mutex_]
    SshAccounts         ssh_accounts_;
    mutable std::mutex  ssh_accounts_mutex_;  // [Config.md#ssh_accounts_mutex_]
    std::wstring        pathname_;
    std::filesystem::file_time_type disk_time_{};  // [Config.md#disk_time_]
    bool                changed_ = false;
    bool                ready_ = false;
    static std::unique_ptr<Config> instance_;
    static std::once_flag init_instance_flag_;

    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    bool interpret_json(boost::property_tree::wptree&& pt);
    bool read_logging_setup(boost::property_tree::wptree&);
    bool read_settings(boost::property_tree::wptree&);
    bool read_ssh_accounts(boost::property_tree::wptree&);
    void write_logging_setup(boost::property_tree::wptree& root);
    void write_settings(boost::property_tree::wptree& root);
    void write_ssh_accounts(boost::property_tree::wptree& root);
    void write_sync_items(boost::property_tree::wptree& root);
    void write_sync_item(const SyncItem& inode, boost::property_tree::wptree& syncitem_node);
    static void init_singleton();
public:
    static Config& instance();
    static std::wstring get_root_pathname();
    static bool exists();
    static bool sync_possible();
    ~Config() = default;
    bool ready() const { return ready_; }
    void add(const SyncItem&, bool loading = false);
    void add(const SshAccount&, bool loading = false);
    void set(unsigned, const SyncItem&);
    bool read(std::wstring_view pathname);
    bool write(const std::wstring& pathname);
    void remove(unsigned);
    bool erase_by_name(const std::wstring& name);
    void pathname(std::wstring_view p);
    const std::wstring& pathname() const { return pathname_; }
    std::optional<SyncItem> find(const std::wstring& path, bool source = true) const;  // [Config.md#find]
    std::optional<SyncItem> find_by_name(const std::wstring& name) const;
    bool update_by_name(const std::wstring& name, const SyncItem& updated);
    SyncItem item(unsigned) const;
    SyncItems syncitems() const;  // [Config.md#syncitems]
    void refresh_last_sync(const std::wstring& name);
    std::optional<SshAccount> find_ssh_account_by_name(const std::string& name) const;  // [Config.md#ssh-account-access]
    std::optional<SshAccount> ssh_account_at(std::size_t index) const;
    SshAccounts ssh_accounts_snapshot() const;
    bool remove_ssh_account(std::size_t index);
    bool update_ssh_account(std::size_t index, const SshAccount& updated);
    bool update_ssh_account_references(const std::string& old_name, const SshAccount& new_account);  // [Config.md#update_ssh_account_references]
    bool ssh_account_referenced_by_any_item(const std::string& name) const;
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    void reset();
    void set_console_mode() { settings_.set_console_mode(); }
    bool console_mode() const { return settings_.console_mode(); }
    bool changed() const { return changed_; }
    bool file_changed_on_disk() const;  // [Config.md#file_changed_on_disk]
    void set_changed() { changed_ = true; }
    void unload();
};

} // namespace AFS_Core
