#include "pch.h"
#include "SyncItem.h"
#include <optional>
#include <utility>
#include <Logging.h>
#include "Translation.h"
#include "SyncFileSsh.h"
#include "util.h"

using std::optional;
using std::nullopt;
using std::wstring;

// FilesystemPath //////////////////////////////////////////////////////////////////////////////////////////////////
FilesystemPath::FilesystemPath()
{
    type(SyncPathType::Filesystem);
}

FilesystemPath::FilesystemPath(const std::wstring& path)
    : path_(path)
{
    type(SyncPathType::Filesystem);
}

std::wstring FilesystemPath::path() const
{
    return path_;
}

void FilesystemPath::setPath(const std::wstring& path)
{
    path_ = path;
}

// SshPath //////////////////////////////////////////////////////////////////////////////////////////////////
SshPath::SshPath()
{
    type(SyncPathType::Ssh);
}

SshPath::SshPath(const SshAccount& account, const std::wstring& path)
    : account_(account)
{
    type(SyncPathType::Ssh);
    // Remove account prefix if present (e.g., "laptop:/home/..." -> "/home/...")
    auto colonPos = path.find(L':');
    if (colonPos != std::wstring::npos && colonPos > 0) {
        // Check if the part before colon matches the account name
        std::wstring prefix = path.substr(0, colonPos);
        std::wstring accountName = util::string_to_wstring(account.name());
        if (prefix == accountName) {
            path_ = path.substr(colonPos + 1);
        } else {
            path_ = path;
        }
    } else {
        path_ = path;
    }
}

std::wstring SshPath::path() const
{
    // Return full SSH path with account name prefix (e.g., "laptop:/tmp").
    // The account name is UTF-8: widening it byte by byte would sign-extend
    // every byte of a non-ASCII character into a wchar_t that no later
    // wstring_to_string() - logging does one per message - can encode.
    return util::string_to_wstring(account_.name()) + L":" + path_;
}

void SshPath::setPath(const std::wstring& path)
{
    path_ = path;
}

const SshAccount& SshPath::account() const
{
    return account_;
}

void SshPath::setAccount(const SshAccount& account)
{
    account_ = account;
}

inline std::string compareItem2Str(CompareItem ci)
{
    switch (ci) {
    case CompareItem::size: return "size";
    case CompareItem::time: return "time";
    case CompareItem::content: return "content";
    }
}

inline optional<CompareItem> str2CompareItem(const std::wstring& s)
{
    if (s == L"size")
        return optional<CompareItem>(CompareItem::size);
    else if (s == L"time")
        return optional<CompareItem>(CompareItem::time);
    else if (s == L"content")
        return optional<CompareItem>(CompareItem::content);
    return nullopt;
}

// The gettext message ids are the German strings the configuration has stored
// since the first version, which is why syncItemTypeKey() can keep writing them
// while syncItemType2Str() shows the translation.
constexpr std::pair<SyncItemType, const char*> syncItemTypeIds[] = {
    {SyncItemType::File, "Datei"},
    {SyncItemType::Dir, "Verzeichnis"},
    {SyncItemType::Tree, "Verzeichnisbaum"},
};

wstring syncItemTypeKey(SyncItemType st)
{
    for (const auto& [type, msgid] : syncItemTypeIds)
        if (type == st)
            return util::string_to_wstring(msgid);
    return L"";
}

wstring syncItemType2Str(SyncItemType st)
{
    for (const auto& [type, msgid] : syncItemTypeIds)
        if (type == st)
            return translation::translate(msgid);
    return L"";
}

SyncItemType str2SyncItemType(const std::wstring& str)
{
    // Both spellings have to be understood: the untranslated key from the
    // configuration file and the translated text a dialog hands back.
    for (const auto& [type, msgid] : syncItemTypeIds)
        if (str == util::string_to_wstring(msgid) || str == translation::translate(msgid))
            return type;
    return SyncItemType::Undefined;
}

// CompareItems /////////////////////////////////////////////////////////////////////////////////////////////////
CompareItems::CompareItems() : std::set<CompareItem>{CompareItem::content}
{}

void CompareItems::set(const std::wstring& name, bool value)
{
    auto ci = str2CompareItem(name);
    if (ci.has_value() && value) 
		insert(ci.value());
}

bool CompareItems::size() const
{
    return find(CompareItem::size) != cend();
}

bool CompareItems::time() const
{
  	return find(CompareItem::time) != cend();
}

bool CompareItems::content() const
{
	return find(CompareItem::content) != cend();
}

void CompareItems::size(bool isSet)
{
	if (isSet)
		insert(CompareItem::size);
	else
		erase(CompareItem::size);
}

void CompareItems::time(bool isSet)
{
	if (isSet)
		insert(CompareItem::time);
	else
		erase(CompareItem::time);
}

void CompareItems::content(bool isSet)
{
	if (isSet)
		insert(CompareItem::content);
	else
		erase(CompareItem::content);
}

// Exclusions /////////////////////////////////////////////////////////////////////////////////////////////////
bool isExcluded(const wstring& name, const Exclusions& exclusions)
{
    for (const auto& pattern : exclusions)
        if (util::wildmatch(name, pattern))
            return true;
    return false;
}

// SyncItem /////////////////////////////////////////////////////////////////////////////////////////////////
SyncItem::SyncItem(const wstring& name, SyncItemType type, const wstring& s, const wstring& t, const wstring& w)
    : name_(name)
    , type_(type)
    , wildcard_(w)
{
    source(s);
    target(t);
}

SyncItem::SyncItem(const wstring& name, SyncItemType type, const wstring& s, const wstring& t, const wstring& w, const Exclusions& e, bool delAftercopy,
                   bool twoWaySync, bool cwtn, bool fsp, bool enabled, const CompareItems& ci)
    : name_(name)
    , type_(type)
    , wildcard_(w)
    , flags_(delAftercopy, twoWaySync, cwtn, fsp, enabled)
    , compareItems_(ci)
    , exclusions_(e)
{
    source(s);
    target(t);
}

SyncItem::SyncItem(const wstring& name, SyncItemType type, const wstring& s, const wstring& t, const wstring& w, const Exclusions& e,
         const SyncFlags& syncFlags, const CompareItems& ci)
    : name_(name)
    , type_(type)
    , wildcard_(w)
    , flags_(syncFlags)
    , compareItems_(ci)
    , exclusions_(e)
{
    source(s);
    target(t);
}

SyncItemType SyncItem::syncItemType() const
{
    return type_;
}

void SyncItem::setLastSync() 
{
    LOG_DEBUG2("SyncItem::setLastSync", std::chrono::steady_clock::now()); 
    lastSync_ = std::chrono::steady_clock::now(); 
}

void SyncItem::source(const std::wstring& s)
{
    LOG_DEBUG(L"SyncItem::source called with: " + s);
    auto accountOpt = getAccountFromPath(s);
    if (accountOpt.has_value()) {
        LOG_DEBUG("SyncItem::source: SSH account found");
        // SshPath strips the account prefix itself; the detour over the narrow
        // getRemotePath() only re-widened the UTF-8 bytes one by one and turned
        // every non-ASCII character into an unencodable wchar_t.
        source_ = std::make_unique<SshPath>(accountOpt.value(), s);
    } else {
        source_ = std::make_unique<FilesystemPath>(s);
    }
}

void SyncItem::target(const std::wstring& t)
{
    LOG_DEBUG(L"SyncItem::target called with: " + t);
    auto accountOpt = getAccountFromPath(t);
    if (accountOpt.has_value()) {
        LOG_DEBUG("SyncItem::target: SSH account found");
        // See SyncItem::source() - SshPath removes the prefix without a
        // narrow/wide round trip.
        target_ = std::make_unique<SshPath>(accountOpt.value(), t);
    } else {
        target_ = std::make_unique<FilesystemPath>(t);
    }
}

// Copy constructor
SyncItem::SyncItem(const SyncItem& other)
    : name_(other.name_)
    , type_(other.type_)
    , wildcard_(other.wildcard_)
    , flags_(other.flags_)
    , compareItems_(other.compareItems_)
    , exclusions_(other.exclusions_)
    , syncCycle_(other.syncCycle_)
    , lastSync_(other.lastSync_)
{
    // Deep copy unique_ptr members
    if (other.source_) {
        if (other.source_->type() == SyncPathType::Filesystem) {
            source_ = std::make_unique<FilesystemPath>(*dynamic_cast<const FilesystemPath*>(other.source_.get()));
        } else {
            source_ = std::make_unique<SshPath>(*dynamic_cast<const SshPath*>(other.source_.get()));
        }
    }
    if (other.target_) {
        if (other.target_->type() == SyncPathType::Filesystem) {
            target_ = std::make_unique<FilesystemPath>(*dynamic_cast<const FilesystemPath*>(other.target_.get()));
        } else {
            target_ = std::make_unique<SshPath>(*dynamic_cast<const SshPath*>(other.target_.get()));
        }
    }
}

SyncItem& SyncItem::operator=(const SyncItem& other)
{
    if (this != &other) {
        name_ = other.name_;
        type_ = other.type_;
        wildcard_ = other.wildcard_;
        flags_ = other.flags_;
        compareItems_ = other.compareItems_;
        exclusions_ = other.exclusions_;
        syncCycle_ = other.syncCycle_;
        lastSync_ = other.lastSync_;
        
        // Deep copy unique_ptr members
        if (other.source_) {
            if (other.source_->type() == SyncPathType::Filesystem) {
                source_ = std::make_unique<FilesystemPath>(*dynamic_cast<const FilesystemPath*>(other.source_.get()));
            } else {
                source_ = std::make_unique<SshPath>(*dynamic_cast<const SshPath*>(other.source_.get()));
            }
        } else {
            source_.reset();
        }
        if (other.target_) {
            if (other.target_->type() == SyncPathType::Filesystem) {
                target_ = std::make_unique<FilesystemPath>(*dynamic_cast<const FilesystemPath*>(other.target_.get()));
            } else {
                target_ = std::make_unique<SshPath>(*dynamic_cast<const SshPath*>(other.target_.get()));
            }
        } else {
            target_.reset();
        }
    }
    return *this;
}

bool SyncItem::renameSshAccountReference(const std::string& oldName, const SshAccount& newAccount)
{
    // source_/target_ hold a *copy* of the SshAccount taken when the path was
    // set (see source()/target() above), not a live lookup - so renaming or
    // editing the account in Config leaves existing items pointing at a name
    // that no longer resolves. Config calls this on every item after a
    // successful account edit to keep them in sync.
    bool changed = false;
    if (source_ && source_->type() == SyncPathType::Ssh) {
        auto* sshSource = dynamic_cast<SshPath*>(source_.get());
        if (sshSource && sshSource->account().name() == oldName) {
            sshSource->setAccount(newAccount);
            changed = true;
        }
    }
    if (target_ && target_->type() == SyncPathType::Ssh) {
        auto* sshTarget = dynamic_cast<SshPath*>(target_.get());
        if (sshTarget && sshTarget->account().name() == oldName) {
            sshTarget->setAccount(newAccount);
            changed = true;
        }
    }
    return changed;
}

bool SyncItem::referencesSshAccount(const std::string& name) const
{
    if (source_ && source_->type() == SyncPathType::Ssh) {
        auto* sshSource = dynamic_cast<const SshPath*>(source_.get());
        if (sshSource && sshSource->account().name() == name)
            return true;
    }
    if (target_ && target_->type() == SyncPathType::Ssh) {
        auto* sshTarget = dynamic_cast<const SshPath*>(target_.get());
        if (sshTarget && sshTarget->account().name() == name)
            return true;
    }
    return false;
}

SSHPATH SyncItem::sshPathType() const
{
    bool sourceIsSsh = source_ && source_->type() == SyncPathType::Ssh;
    bool targetIsSsh = target_ && target_->type() == SyncPathType::Ssh;
    if (sourceIsSsh && targetIsSsh)
        return SSHPATH::both;
    else if (sourceIsSsh)
        return SSHPATH::source;
    else if (targetIsSsh)
        return SSHPATH::dest;
    else
        return SSHPATH::none;
}