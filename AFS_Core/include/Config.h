#pragma once

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <optional>
#include <mutex>
#include "SyncItem.h"
#include "Settings.h"
#include "SshAccount.h"
#include <boost/property_tree/json_parser.hpp>

namespace AFS_Core {

using SshAccounts = std::vector<SshAccount>;

class Config {
    Settings    settings_;
    SyncItems   syncitems_;
    mutable std::mutex syncitems_mutex_;   // guards syncitems_: GUI thread (add/remove/edit sync items) mutates it
                                           // while the sync worker thread reads a snapshot of it concurrently
    SshAccounts ssh_accounts_;
    mutable std::mutex ssh_accounts_mutex_; // guards ssh_accounts_: GUI thread (SshAccountsDialog) mutates it
                                           // while the sync worker thread searches it concurrently
    std::wstring pathname_;
    bool changed_ = false;
    bool ready_ = false;
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
    // find/find_by_name return copies, not pointers into syncitems_: a raw pointer
    // would be written by the GUI thread without the lock (and could dangle after
    // add/remove reallocates) while the sync thread snapshots the vector.
    // Mutations go through update_by_name()/erase_by_name() instead.
    std::optional<SyncItem> find(const std::wstring& path, bool source = true) const;
    std::optional<SyncItem> find_by_name(const std::wstring& name) const;
    bool update_by_name(const std::wstring& name, const SyncItem& updated);
    SyncItem item(unsigned) const;
    // Returns a snapshot copy, not a reference: the sync worker thread reads this
    // concurrently with GUI-thread add()/set()/remove()/erase() calls, and SyncDir/
    // SyncFile/SyncTree hold their SyncItem for the duration of a (possibly slow)
    // copy, so a live reference into syncitems_ could be reallocated/erased from
    // under them.
    SyncItems syncitems() const;
    void refresh_last_sync(const std::wstring& name);
    // No direct ssh_accounts_ reference is exposed: callers on different threads
    // must not iterate/mutate the shared vector without going through the lock.
    std::optional<SshAccount> find_ssh_account_by_name(const std::string& name) const;
    std::optional<SshAccount> ssh_account_at(std::size_t index) const;
    SshAccounts ssh_accounts_snapshot() const;
    bool remove_ssh_account(std::size_t index);
    bool update_ssh_account(std::size_t index, const SshAccount& updated);
    // Keeps sync items pointing at a renamed/edited SSH account working: their
    // embedded account snapshot (see SyncItem::rename_ssh_account_reference) is
    // updated in place wherever it still matches oldName. Call after a
    // successful update_ssh_account() whose old name you captured beforehand.
    bool update_ssh_account_references(const std::string& old_name, const SshAccount& new_account);
    bool ssh_account_referenced_by_any_item(const std::string& name) const;
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    void reset();
    void set_console_mode() { settings_.set_console_mode(); }
    bool console_mode() const { return settings_.console_mode(); }
    bool changed() const { return changed_; }
    void set_changed() { changed_ = true; }
    void unload();
};

} // namespace AFS_Core
