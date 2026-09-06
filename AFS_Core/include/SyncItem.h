#pragma once

#include <string>
#include <set>
#include <vector>
#include <chrono>
#include "SshAccount.h"

namespace AFS_Core {

enum class SSHPATH {none, source, dest, both};

class SyncFlags {
    bool    delAftercopy_ = false;
    bool    twoWaySync_ = false;
    bool    copyWhenTargetNewer_ = false;
    bool    fatherSonPrinciple_ = false;
    bool    deleteInTarget_ = false;
    bool    backupProof_ = false;
    // Leere Dateien werden ohne dieses Flag uebersprungen: eine Datei der
    // Groesse 0 ist meist ein abgebrochener Schreibvorgang, und sie ueber eine
    // vorhandene Zieldatei zu kopieren waere Datenverlust.
    bool    syncEmptyFiles_ = false;
    bool    enabled_ = true;
    uint16_t syncCycle_ = 60;
public:
    SyncFlags() = default;
    SyncFlags(bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled);
    bool delAftercopy() const;
    void delAftercopy(bool v);
    bool twoWaySync() const;
    void twoWaySync(bool v);
    bool copyWhenTargetNewer() const;
    void copyWhenTargetNewer(bool v);
    bool fatherSonPrinciple() const;
    void fatherSonPrinciple(bool v);
    bool deleteInTarget() const;
    void deleteInTarget(bool v);
    bool backupProof() const;
    void backupProof(bool v);
    bool syncEmptyFiles() const;
    void syncEmptyFiles(bool v);
    bool enabled() const;
    void enabled(bool v);
};

enum class CompareItem { size, time, content };
class CompareItems: std::set<CompareItem>{
public:
	CompareItems();
    void set(const std::wstring& name, bool value);
    bool size() const;
    bool time() const;
    bool content() const;
    void size(bool);
    void time(bool);
    void content(bool);
};

enum class SyncPathType { Filesystem, Ssh };

class SyncPath {
    SyncPathType type_;
public:
    virtual ~SyncPath() = default;
    virtual std::wstring path() const = 0;
    SyncPathType type() const;
    void type(SyncPathType t);
};

class FilesystemPath: public SyncPath {
    std::wstring path_;
public:
    FilesystemPath();
    FilesystemPath(const std::wstring& path);
    std::wstring path() const override;
    void setPath(const std::wstring& path);
};

class SshPath: public SyncPath {
    SshAccount account_;
    std::wstring path_;
public:
    SshPath();
    SshPath(const SshAccount& account, const std::wstring& path);
    std::wstring path() const override;
    void setPath(const std::wstring& path);
    const SshAccount& account() const;
    void setAccount(const SshAccount& account);
};

class Exclusions: public std::vector<std::wstring> {
public:
};

// True if `name` (a filename or directory name, not a full path - matching
// how the existing wildcard field is applied) matches any exclusion pattern.
// Patterns use the same */? wildcard syntax as the wildcard field itself.
bool isExcluded(const std::wstring& name, const Exclusions& exclusions);

enum class SyncItemType {
    Undefined,
    File,
    Dir,
    Tree
};

std::wstring syncItemType2Str(SyncItemType);       // translated, for display
std::wstring syncItemTypeKey(SyncItemType);        // untranslated, for the configuration file
SyncItemType str2SyncItemType(const std::wstring&);

class SyncItem {
    using timePoint = std::chrono::steady_clock::time_point;
    using SyncPathPtr = std::unique_ptr<SyncPath>;
    std::wstring    name_;  // key field
    SyncItemType    type_ = SyncItemType::Undefined;
    SyncPathPtr     source_;
    SyncPathPtr     target_;
    std::wstring    wildcard_;
    SyncFlags       flags_;
    CompareItems    compareItems_;
    Exclusions		exclusions_;
    uint16_t        syncCycle_ = 60;
    timePoint       lastSync_ = (timePoint::min)(); // non persitent data - initialize to min so first sync runs immediately
public:
    SyncItem(){}
    SyncItem(const SyncItem& other);
    SyncItem& operator=(const SyncItem& other);
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w);
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w, const Exclusions& e,
             bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled, const CompareItems&);
    SyncItem(const std::wstring& name, SyncItemType type, const std::wstring& s, const std::wstring& t, const std::wstring& w, const Exclusions& e,
             const SyncFlags& syncFlags, const CompareItems&);
    bool operator==(const SyncItem& o) const;
    std::wstring name() const;
    void name(const std::wstring& n);
    std::wstring source() const;
    bool sshSource() const;
    void source(const std::wstring& s);
    std::wstring target() const;
    bool sshTarget() const;
    void target(const std::wstring& t);
    const SyncPath* sourcePath() const;
    const SyncPath* targetPath() const;
    void setSourcePath(SyncPathPtr path);
    void setTargetPath(SyncPathPtr path);
    std::wstring wildcard() const;
    void wildcard(const std::wstring& w);
    bool delAftercopy() const;
    void delAftercopy(bool d);
    bool twoWaySync() const;
    void twoWaySync(bool t);
    bool copyWhenTargetNewer() const;
    void copyWhenTargetNewer(bool c);
    bool fatherSonPrinciple() const;
    void fatherSonPrinciple(bool f);
    bool deleteInTarget() const;
    void deleteInTarget(bool d);
    bool backupProof() const;
    void backupProof(bool b);
    bool syncEmptyFiles() const;
    void syncEmptyFiles(bool s);
    bool enabled() const;
    void enabled(bool e);
    SyncItemType syncItemType() const;
    std::wstring itemTypeStr() const;
    void syncItemType(SyncItemType t);
    SyncFlags flags() const;
    void flags(const SyncFlags& sf);
    CompareItems compareItems() const;
    void compareItems(const CompareItems& ci);
    void addExclusion(const std::wstring& excl);
    void exclusions(const Exclusions& exc);
    Exclusions exclusions() const;
    void syncCycle(uint16_t p);
    uint16_t syncCycle() const;
    void setLastSync();
    timePoint lastSync() const;
    SSHPATH sshPathType() const;
    // Updates an embedded SSH account reference in-place (path unchanged) when
    // it matches oldName - see Config::updateSshAccountReferences() for why.
    bool renameSshAccountReference(const std::string& oldName, const SshAccount& newAccount);
    bool referencesSshAccount(const std::string& name) const;
};

using SyncItems=std::vector<SyncItem>;

// SyncFlags inline implementations
inline bool SyncFlags::delAftercopy() const { return delAftercopy_; }
inline void SyncFlags::delAftercopy(bool v) { delAftercopy_ = v; }
inline bool SyncFlags::twoWaySync() const { return twoWaySync_; }
inline void SyncFlags::twoWaySync(bool v) { twoWaySync_ = v; }
inline bool SyncFlags::copyWhenTargetNewer() const { return copyWhenTargetNewer_; }
inline void SyncFlags::copyWhenTargetNewer(bool v) { copyWhenTargetNewer_ = v; }
inline bool SyncFlags::fatherSonPrinciple() const { return fatherSonPrinciple_; }
inline void SyncFlags::fatherSonPrinciple(bool v) { fatherSonPrinciple_ = v; }
inline bool SyncFlags::deleteInTarget() const { return deleteInTarget_; }
inline void SyncFlags::deleteInTarget(bool v) { deleteInTarget_ = v; }
inline bool SyncFlags::backupProof() const { return backupProof_; }
inline void SyncFlags::backupProof(bool v) { backupProof_ = v; }
inline bool SyncFlags::syncEmptyFiles() const { return syncEmptyFiles_; }
inline void SyncFlags::syncEmptyFiles(bool v) { syncEmptyFiles_ = v; }
inline bool SyncFlags::enabled() const { return enabled_; }
inline void SyncFlags::enabled(bool v) { enabled_ = v; }

// SyncPath inline implementations
inline SyncPathType SyncPath::type() const { return type_; }
inline void SyncPath::type(SyncPathType t) { type_ = t; }

// SyncItem inline implementations
inline bool SyncItem::operator==(const SyncItem& o) const { return o.name_ == name_; }
inline std::wstring SyncItem::name() const { return name_; }
inline void SyncItem::name(const std::wstring& n) { name_ = n; }
inline std::wstring SyncItem::source() const { return source_ ? source_->path() : L""; }
inline bool SyncItem::sshSource() const { return source_ && source_->type() == SyncPathType::Ssh; }
inline std::wstring SyncItem::target() const { return target_ ? target_->path() : L""; }
inline bool SyncItem::sshTarget() const { return target_ && target_->type() == SyncPathType::Ssh; }
inline const SyncPath* SyncItem::sourcePath() const { return source_.get(); }
inline const SyncPath* SyncItem::targetPath() const { return target_.get(); }
inline void SyncItem::setSourcePath(SyncPathPtr path) { source_ = std::move(path); }
inline void SyncItem::setTargetPath(SyncPathPtr path) { target_ = std::move(path); }
inline std::wstring SyncItem::wildcard() const { return wildcard_; }
inline void SyncItem::wildcard(const std::wstring& w) { wildcard_ = w; }
inline bool SyncItem::delAftercopy() const { return flags_.delAftercopy(); }
inline void SyncItem::delAftercopy(bool d) { flags_.delAftercopy(d); }
inline bool SyncItem::twoWaySync() const { return flags_.twoWaySync(); }
inline void SyncItem::twoWaySync(bool t) { flags_.twoWaySync(t); }
inline bool SyncItem::copyWhenTargetNewer() const { return flags_.copyWhenTargetNewer(); }
inline void SyncItem::copyWhenTargetNewer(bool c) { flags_.copyWhenTargetNewer(c); }
inline bool SyncItem::fatherSonPrinciple() const { return flags_.fatherSonPrinciple(); }
inline void SyncItem::fatherSonPrinciple(bool f) { flags_.fatherSonPrinciple(f); }
inline bool SyncItem::deleteInTarget() const { return flags_.deleteInTarget(); }
inline void SyncItem::deleteInTarget(bool d) { flags_.deleteInTarget(d); }
inline bool SyncItem::backupProof() const { return flags_.backupProof(); }
inline void SyncItem::backupProof(bool b) { flags_.backupProof(b); }
inline bool SyncItem::syncEmptyFiles() const { return flags_.syncEmptyFiles(); }
inline void SyncItem::syncEmptyFiles(bool s) { flags_.syncEmptyFiles(s); }
inline bool SyncItem::enabled() const { return flags_.enabled(); }
inline void SyncItem::enabled(bool e) { flags_.enabled(e); }
inline std::wstring SyncItem::itemTypeStr() const { return syncItemType2Str(type_); }
inline void SyncItem::syncItemType(SyncItemType t) { type_ = t; }
inline SyncFlags SyncItem::flags() const { return flags_; }
inline void SyncItem::flags(const SyncFlags& sf) { flags_ = sf; }
inline CompareItems SyncItem::compareItems() const { return compareItems_; }
inline void SyncItem::compareItems(const CompareItems& ci) { compareItems_ = ci; }
inline void SyncItem::addExclusion(const std::wstring& excl) { exclusions_.push_back(excl); }
inline void SyncItem::exclusions(const Exclusions& exc) { exclusions_ = exc; }
inline Exclusions SyncItem::exclusions() const { return exclusions_; }
inline void SyncItem::syncCycle(uint16_t p) { syncCycle_ = p; }
inline uint16_t SyncItem::syncCycle() const { return syncCycle_; }
inline SyncItem::timePoint SyncItem::lastSync() const { return lastSync_; }

} // namespace AFS_Core
