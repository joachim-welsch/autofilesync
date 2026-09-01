#pragma once

#include <string>
#include <set>
#include <vector>
#include <chrono>

class SyncFlags {
    bool                    delAftercopy_ = false;
    bool                    twoWaySync_ = false;
    bool                    copyWhenTargetNewer_ = false;
    bool                    fatherSonPrinciple_ = false;
    bool                    enabled_ = true;
    std::chrono::seconds    syncInterval_ = {std::chrono::seconds(1)};
public:
    SyncFlags() = default;
    SyncFlags(bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled);
    bool delAftercopy() const { return delAftercopy_; }
    bool twoWaySync() const { return twoWaySync_; }
    bool copyWhenTargetNewer() const { return copyWhenTargetNewer_; }
    bool fatherSonPrinciple() const { return fatherSonPrinciple_; }
    bool enabled() const { return enabled_; }
    void enabled(bool e) { enabled_ = e; }
};

enum class CompareItem { size, time, content, invalid };
using CompareItems = std::set<CompareItem>;

inline std::string compareItem2Str(CompareItem ci)
{
    switch (ci) {
        case CompareItem::size: return "size";
        case CompareItem::time: return "time";
        case CompareItem::content: return "content";
    default: return "";
    }
}

inline CompareItem str2CompareItem(const std::string& s)
{
    if (s=="size")
        return CompareItem::size;
    else if (s=="time")
        return CompareItem::time;
    else if (s=="content")
        return CompareItem::content;
    else
        return CompareItem::invalid;
}

class Exclusions: public std::vector<std::string> {
public:
};

class SyncItem {
    std::string     type_;
    std::string     source_;
    std::string     target_;
    std::string     wildcard_;
    SyncFlags       flags_;
    CompareItems    compareItems_;
    Exclusions		exclusions_;
public:
    SyncItem(){}
    SyncItem(const std::string& type, const std::string& s, const std::string& t, const std::string& w, const Exclusions& e,
             bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled, const CompareItems&);
    SyncItem(const std::string& type, const std::string& s, const std::string& t, const std::string& w, const Exclusions& e,
             const SyncFlags& syncFlags, const CompareItems&);
    void source(const std::string& s) { source_=s;}
    void target(const std::string& t) { target_=t;}
    void wildcard(const std::string& w) { wildcard_=w;}
    std::string source() const { return source_; }
    std::string target() const { return target_; }
    bool delAftercopy() const { return flags_.delAftercopy(); }
    bool twoWaySync() const { return flags_.twoWaySync(); }
    bool copyWhenTargetNewer() const { return flags_.copyWhenTargetNewer(); }
    bool fatherSonPrinciple() const { return flags_.fatherSonPrinciple(); }
    bool enabled() const { return flags_.enabled(); }
    void enabled(bool e) { flags_.enabled(e); }
    std::string typeStr() const;
    std::string wildcard() const { return wildcard_; }
    SyncFlags flags() const { return flags_; }
    CompareItems compareItems() const { return compareItems_; }
    void compareItems(const CompareItems& ci) { compareItems_=ci; }
    void addExclusion(const std::string& excl) { exclusions_.push_back(excl);  }
    Exclusions exclusions() const { return exclusions_; }
};

using SyncItems=std::vector<SyncItem>;
