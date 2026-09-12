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

SyncFlags::SyncFlags(bool del_aftercopy, bool tws, bool cwtn, bool fsp, bool enabled)
    : del_aftercopy_(del_aftercopy)
    , two_way_sync_(tws)
    , copy_when_target_newer_(cwtn)
    , father_son_principle_(fsp)
    , enabled_(enabled)
{}

FilesystemPath::FilesystemPath()
    : SyncPath(SyncPathType::Filesystem)
{}

FilesystemPath::FilesystemPath(const std::wstring& path)
    : SyncPath(SyncPathType::Filesystem)
    , path_(path)
{}

std::wstring FilesystemPath::path() const
{
    return path_;
}

std::unique_ptr<SyncPath> FilesystemPath::clone() const
{
    return std::make_unique<FilesystemPath>(*this);
}

void FilesystemPath::set_path(const std::wstring& path)
{
    path_ = path;
}

SshPath::SshPath()
    : SyncPath(SyncPathType::Ssh)
{}

SshPath::SshPath(const SshAccount& account, const std::wstring& path)  // [SyncItem.md#SshPath-strips-account-prefix]
    : SyncPath(SyncPathType::Ssh)
    , account_(account)
    , path_(path)
{
    const std::wstring prefix = util::string_to_wstring(account.name()) + L":";
    if (path.starts_with(prefix))
        path_ = path.substr(prefix.size());
}

std::wstring SshPath::path() const
{
    return util::string_to_wstring(account_.name()) + L":" + path_;
}

std::unique_ptr<SyncPath> SshPath::clone() const
{
    return std::make_unique<SshPath>(*this);
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

static optional<CompareItem> str_to_compare_item(const std::wstring& s)
{
    if (s == L"size")
        return CompareItem::size;
    if (s == L"time")
        return CompareItem::time;
    if (s == L"content")
        return CompareItem::content;
    return nullopt;
}

constexpr std::pair<SyncItemType, const char*> SYNC_ITEM_TYPE_IDS[] = {  // [SyncItem.md#SYNC_ITEM_TYPE_IDS]
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
    for (const auto& [type, msgid] : SYNC_ITEM_TYPE_IDS)  // [SyncItem.md#sync_item_type_from_str]
        if (str == util::string_to_wstring(msgid) || str == translation::translate(msgid))
            return type;
    return SyncItemType::Undefined;
}

CompareItems::CompareItems() : std::set<CompareItem>{CompareItem::content}
{}

void CompareItems::set(const std::wstring& name, bool value)
{
    const auto ci = str_to_compare_item(name);
    if (ci.has_value() && value)
        insert(ci.value());
}

bool CompareItems::size() const
{
    return contains(CompareItem::size);
}

bool CompareItems::time() const
{
    return contains(CompareItem::time);
}

bool CompareItems::content() const
{
    return contains(CompareItem::content);
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

bool is_excluded(const wstring& name, const Exclusions& exclusions)
{
    for (const auto& pattern : exclusions)
        if (util::wildmatch(name, pattern))
            return true;
    return false;
}

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
    last_sync_ = std::chrono::steady_clock::now();
    log_debug("SyncItem::set_last_sync", last_sync_);
}

void SyncItem::source(const std::wstring& s)
{
    log_debug(L"SyncItem::source called with: " + s);
    auto account_opt = get_account_from_path(s);
    if (account_opt.has_value()) {
        log_debug("SyncItem::source: SSH account found");
        source_ = std::make_unique<SshPath>(account_opt.value(), s);  // [SyncItem.md#ssh-path-without-round-trip]
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
        target_ = std::make_unique<SshPath>(account_opt.value(), t);  // [SyncItem.md#ssh-path-without-round-trip]
    } else {
        target_ = std::make_unique<FilesystemPath>(t);
    }
}

SyncItem::SyncItem(const SyncItem& other)
    : name_(other.name_)
    , type_(other.type_)
    , source_(other.source_ ? other.source_->clone() : nullptr)
    , target_(other.target_ ? other.target_->clone() : nullptr)
    , wildcard_(other.wildcard_)
    , flags_(other.flags_)
    , compare_items_(other.compare_items_)
    , exclusions_(other.exclusions_)
    , sync_cycle_(other.sync_cycle_)
    , last_sync_(other.last_sync_)
{}

SyncItem& SyncItem::operator=(const SyncItem& other)
{
    if (this != &other)
        *this = SyncItem(other);
    return *this;
}

SshPath* SyncItem::ssh_path(const std::unique_ptr<SyncPath>& path) const
{
    return path && path->type() == SyncPathType::Ssh ? dynamic_cast<SshPath*>(path.get()) : nullptr;
}

bool SyncItem::rename_ssh_account_reference(const std::string& old_name, const SshAccount& new_account)
{
    bool changed = false;  // [SyncItem.md#rename-account-references]
    for (SshPath* path : { ssh_path(source_), ssh_path(target_) })
        if (path && path->account().name() == old_name) {
            path->set_account(new_account);
            changed = true;
        }
    return changed;
}

bool SyncItem::references_ssh_account(const std::string& name) const
{
    for (const SshPath* path : { ssh_path(source_), ssh_path(target_) })
        if (path && path->account().name() == name)
            return true;
    return false;
}

SSHPATH SyncItem::ssh_path_type() const
{
    if (ssh_source() && ssh_target())
        return SSHPATH::both;
    if (ssh_source())
        return SSHPATH::source;
    if (ssh_target())
        return SSHPATH::dest;
    return SSHPATH::none;
}

} // namespace AFS_Core
