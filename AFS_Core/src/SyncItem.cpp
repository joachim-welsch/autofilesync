#include "pch.h"
#include "SyncItem.h"
#include <optional>
#include <utility>
#include <Logging.h>
#include "Translation.h"
#include "SyncFileSsh.h"
#include "Util.h"

namespace AFS_Core {

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

void FilesystemPath::set_path(const std::wstring& path)
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
    auto colon_pos = path.find(L':');
    if (colon_pos != std::wstring::npos && colon_pos > 0) {
        // Check if the part before colon matches the account name
        std::wstring prefix = path.substr(0, colon_pos);
        std::wstring account_name = util::string_to_wstring(account.name());
        if (prefix == account_name) {
            path_ = path.substr(colon_pos + 1);
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

void SshPath::set_path(const std::wstring& path)
{
    path_ = path;
}

const SshAccount& SshPath::account() const
{
    return account_;
}

void SshPath::set_account(const SshAccount& account)
{
    account_ = account;
}

inline std::string compare_item_to_str(CompareItem ci)
{
    switch (ci) {
    case CompareItem::size: return "size";
    case CompareItem::time: return "time";
    case CompareItem::content: return "content";
    }
}

inline optional<CompareItem> str_to_compare_item(const std::wstring& s)
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
// since the first version, which is why sync_item_type_key() can keep writing them
// while sync_item_type_to_str() shows the translation.
constexpr std::pair<SyncItemType, const char*> SYNC_ITEM_TYPE_IDS[] = {
    {SyncItemType::File, "Datei"},
    {SyncItemType::Dir, "Verzeichnis"},
    {SyncItemType::Tree, "Verzeichnisbaum"},
};

wstring sync_item_type_key(SyncItemType st)
{
    for (const auto& [type, msgid] : SYNC_ITEM_TYPE_IDS)
        if (type == st)
            return util::string_to_wstring(msgid);
    return L"";
}

wstring sync_item_type_to_str(SyncItemType st)
{
    for (const auto& [type, msgid] : SYNC_ITEM_TYPE_IDS)
        if (type == st)
            return translation::translate(msgid);
    return L"";
}

SyncItemType str_to_sync_item_type(const std::wstring& str)
{
    // Both spellings have to be understood: the untranslated key from the
    // configuration file and the translated text a dialog hands back.
    for (const auto& [type, msgid] : SYNC_ITEM_TYPE_IDS)
        if (str == util::string_to_wstring(msgid) || str == translation::translate(msgid))
            return type;
    return SyncItemType::Undefined;
}

// CompareItems /////////////////////////////////////////////////////////////////////////////////////////////////
CompareItems::CompareItems() : std::set<CompareItem>{CompareItem::content}
{}

void CompareItems::set(const std::wstring& name, bool value)
{
    auto ci = str_to_compare_item(name);
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

void CompareItems::size(bool is_set)
{
	if (is_set)
		insert(CompareItem::size);
	else
		erase(CompareItem::size);
}

void CompareItems::time(bool is_set)
{
	if (is_set)
		insert(CompareItem::time);
	else
		erase(CompareItem::time);
}

void CompareItems::content(bool is_set)
{
	if (is_set)
		insert(CompareItem::content);
	else
		erase(CompareItem::content);
}

// Exclusions /////////////////////////////////////////////////////////////////////////////////////////////////
bool is_excluded(const wstring& name, const Exclusions& exclusions)
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

SyncItem::SyncItem(const wstring& name, SyncItemType type, const wstring& s, const wstring& t, const wstring& w, const Exclusions& e, bool del_aftercopy,
                   bool two_way_sync, bool cwtn, bool fsp, bool enabled, const CompareItems& ci)
    : name_(name)
    , type_(type)
    , wildcard_(w)
    , flags_(del_aftercopy, two_way_sync, cwtn, fsp, enabled)
    , compare_items_(ci)
    , exclusions_(e)
{
    source(s);
    target(t);
}

SyncItem::SyncItem(const wstring& name, SyncItemType type, const wstring& s, const wstring& t, const wstring& w, const Exclusions& e,
         const SyncFlags& sync_flags, const CompareItems& ci)
    : name_(name)
    , type_(type)
    , wildcard_(w)
    , flags_(sync_flags)
    , compare_items_(ci)
    , exclusions_(e)
{
    source(s);
    target(t);
}

SyncItemType SyncItem::sync_item_type() const
{
    return type_;
}

void SyncItem::set_last_sync() 
{
    log_debug("SyncItem::set_last_sync", std::chrono::steady_clock::now()); 
    last_sync_ = std::chrono::steady_clock::now(); 
}

void SyncItem::source(const std::wstring& s)
{
    log_debug(L"SyncItem::source called with: " + s);
    auto account_opt = get_account_from_path(s);
    if (account_opt.has_value()) {
        log_debug("SyncItem::source: SSH account found");
        // SshPath strips the account prefix itself; the detour over the narrow
        // get_remote_path() only re-widened the UTF-8 bytes one by one and turned
        // every non-ASCII character into an unencodable wchar_t.
        source_ = std::make_unique<SshPath>(account_opt.value(), s);
    } else {
        source_ = std::make_unique<FilesystemPath>(s);
    }
}

void SyncItem::target(const std::wstring& t)
{
    log_debug(L"SyncItem::target called with: " + t);
    auto account_opt = get_account_from_path(t);
    if (account_opt.has_value()) {
        log_debug("SyncItem::target: SSH account found");
        // See SyncItem::source() - SshPath removes the prefix without a
        // narrow/wide round trip.
        target_ = std::make_unique<SshPath>(account_opt.value(), t);
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
    , compare_items_(other.compare_items_)
    , exclusions_(other.exclusions_)
    , sync_cycle_(other.sync_cycle_)
    , last_sync_(other.last_sync_)
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
        compare_items_ = other.compare_items_;
        exclusions_ = other.exclusions_;
        sync_cycle_ = other.sync_cycle_;
        last_sync_ = other.last_sync_;
        
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

bool SyncItem::rename_ssh_account_reference(const std::string& old_name, const SshAccount& new_account)
{
    // source_/target_ hold a *copy* of the SshAccount taken when the path was
    // set (see source()/target() above), not a live lookup - so renaming or
    // editing the account in Config leaves existing items pointing at a name
    // that no longer resolves. Config calls this on every item after a
    // successful account edit to keep them in sync.
    bool changed = false;
    if (source_ && source_->type() == SyncPathType::Ssh) {
        auto* ssh_source = dynamic_cast<SshPath*>(source_.get());
        if (ssh_source && ssh_source->account().name() == old_name) {
            ssh_source->set_account(new_account);
            changed = true;
        }
    }
    if (target_ && target_->type() == SyncPathType::Ssh) {
        auto* ssh_target = dynamic_cast<SshPath*>(target_.get());
        if (ssh_target && ssh_target->account().name() == old_name) {
            ssh_target->set_account(new_account);
            changed = true;
        }
    }
    return changed;
}

bool SyncItem::references_ssh_account(const std::string& name) const
{
    if (source_ && source_->type() == SyncPathType::Ssh) {
        auto* ssh_source = dynamic_cast<const SshPath*>(source_.get());
        if (ssh_source && ssh_source->account().name() == name)
            return true;
    }
    if (target_ && target_->type() == SyncPathType::Ssh) {
        auto* ssh_target = dynamic_cast<const SshPath*>(target_.get());
        if (ssh_target && ssh_target->account().name() == name)
            return true;
    }
    return false;
}

SSHPATH SyncItem::ssh_path_type() const
{
    bool source_is_ssh = source_ && source_->type() == SyncPathType::Ssh;
    bool target_is_ssh = target_ && target_->type() == SyncPathType::Ssh;
    if (source_is_ssh && target_is_ssh)
        return SSHPATH::both;
    else if (source_is_ssh)
        return SSHPATH::source;
    else if (target_is_ssh)
        return SSHPATH::dest;
    else
        return SSHPATH::none;
}

} // namespace AFS_Core
