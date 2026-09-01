#pragma once

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <optional>
#include <mutex>
#include "syncitem.h"
#include "settings.h"
#include "SshAccount.h"
#include <boost/property_tree/json_parser.hpp>

using SshAccounts = std::vector<SshAccount>;

class Config {
    Settings    settings_;
    SyncItems   syncitems_;
    mutable std::mutex syncitemsMutex_;   // guards syncitems_: GUI thread (add/remove/edit sync items) mutates it
                                           // while the sync worker thread reads a snapshot of it concurrently
    SshAccounts sshAccounts_;
    mutable std::mutex sshAccountsMutex_; // guards sshAccounts_: GUI thread (SshAccountsDialog) mutates it
                                           // while the sync worker thread searches it concurrently
    std::wstring pathname_;
    bool changed_ = false;
    bool ready_ = false;
    static std::unique_ptr<Config> instance_;
    static std::once_flag initInstanceFlag_;

    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    bool interpretJson(boost::property_tree::wptree&& pt);
    bool readLoggingSetup(boost::property_tree::wptree&);
    bool readSettings(boost::property_tree::wptree&);
    bool readSshAccounts(boost::property_tree::wptree&);
    void writeLoggingSetup(boost::property_tree::wptree& root);
    void writeSettings(boost::property_tree::wptree& root);
    void writeSshAccounts(boost::property_tree::wptree& root);
    void writeSyncItems(boost::property_tree::wptree& root);
    void writeSyncItem(const SyncItem& inode, boost::property_tree::wptree& syncitemNode);
    static void initSingleton();
public:
    static Config& instance();
    static std::wstring getRootPathname();
    static bool exists();
    static bool syncPossible();
    ~Config() = default;
    bool ready() const { return ready_; }
    void add(const SyncItem&, bool loading = false);
    void add(const SshAccount&, bool loading = false);
    void set(unsigned, const SyncItem&);
    bool read(std::wstring_view pathname);
    bool write(const std::wstring& pathname);
    void remove(unsigned);
    bool eraseByName(const std::wstring& name);
    void pathname(std::wstring_view p);
    const std::wstring& pathname() const { return pathname_; }
    // find/findByName return copies, not pointers into syncitems_: a raw pointer
    // would be written by the GUI thread without the lock (and could dangle after
    // add/remove reallocates) while the sync thread snapshots the vector.
    // Mutations go through updateByName()/eraseByName() instead.
    std::optional<SyncItem> find(const std::wstring& path, bool source = true) const;
    std::optional<SyncItem> findByName(const std::wstring& name) const;
    bool updateByName(const std::wstring& name, const SyncItem& updated);
    SyncItem item(unsigned) const;
    // Returns a snapshot copy, not a reference: the sync worker thread reads this
    // concurrently with GUI-thread add()/set()/remove()/erase() calls, and SyncDir/
    // SyncFile/SyncTree hold their SyncItem for the duration of a (possibly slow)
    // copy, so a live reference into syncitems_ could be reallocated/erased from
    // under them.
    SyncItems syncitems() const;
    void refreshLastSync(const std::wstring& name);
    // No direct sshAccounts_ reference is exposed: callers on different threads
    // must not iterate/mutate the shared vector without going through the lock.
    std::optional<SshAccount> findSshAccountByName(const std::string& name) const;
    std::optional<SshAccount> sshAccountAt(std::size_t index) const;
    SshAccounts sshAccountsSnapshot() const;
    bool removeSshAccount(std::size_t index);
    bool updateSshAccount(std::size_t index, const SshAccount& updated);
    // Keeps sync items pointing at a renamed/edited SSH account working: their
    // embedded account snapshot (see SyncItem::renameSshAccountReference) is
    // updated in place wherever it still matches oldName. Call after a
    // successful updateSshAccount() whose old name you captured beforehand.
    bool updateSshAccountReferences(const std::string& oldName, const SshAccount& newAccount);
    bool sshAccountReferencedByAnyItem(const std::string& name) const;
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    void reset();
    void setConsoleMode() { settings_.setConsoleMode(); }
    bool consoleMode() const { return settings_.consoleMode(); }
    bool changed() const { return changed_; }
    void setChanged() { changed_ = true; }
    void unload();
};
