#pragma once

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <optional>
#include "syncitem.h"
#include "settings.h"
#include <boost/property_tree/json_parser.hpp>
#include <mutex>

class Config {
    Settings    settings_;
    SyncItems   syncitems_;
    std::wstring pathname_;
    bool changed_ = false;
    bool ready_ = false;
    static Config* instance_;
    static std::once_flag initInstanceFlag_;

    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    bool interpretJson(boost::property_tree::wptree&& pt);
    bool readLoggingSetup(boost::property_tree::wptree&);
    bool readSettings(boost::property_tree::wptree&);
    void writeLoggingSetup(boost::property_tree::wptree& root);
    void writeSettings(boost::property_tree::wptree& root);
    void writeSyncItems(boost::property_tree::wptree& root);
    void writeSyncItem(const SyncItem& inode, boost::property_tree::wptree& syncitemNode);
    static void initSingleton() {
        instance_ = new Config;
    }
public:
    static Config& instance()
    {
        std::call_once(initInstanceFlag_, &Config::initSingleton);
        return *instance_;
    }
    static std::wstring getRootPathname();
    ~Config() = default;
    bool ready() const { return ready_; }
    void add(const SyncItem&, bool loading = false);
    void set(unsigned, const SyncItem&);
    bool read(const std::wstring& pathname);
    bool write(const std::wstring& pathname);
    void remove(unsigned);
    bool erase(SyncItem* item);
    void pathname(const std::wstring& p);
    const std::wstring& pathname() const { return pathname_; }
    std::optional<SyncItem*> find(const std::wstring& path, bool source = true);
    std::optional<SyncItem*> findByName(const std::wstring& name);
    const SyncItem& item(unsigned) const;
    const SyncItems& syncitems() const { return syncitems_; }
    SyncItems& syncItems() { return syncitems_; }
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    void reset();
    void setConsoleMode() { settings_.setConsoleMode(); }
    bool consoleMode() const { return settings_.consoleMode(); }
    bool changed() const { return changed_; }
    void setChanged() { changed_ = true; }
};
