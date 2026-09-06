#include "pch.h"
#include "Config.h"
#include "util.h"
#include "PasswordCrypto.h"
#include <fstream>
#include <string>
#include <iostream>
#include <mutex>
#ifdef _WXWIDGETS
    #include "wx/msgdlg.h"
#endif
#include <stdlib.h>
#include <FilesystemUtil.h>
#include "Logging.h"
#include "RecycleBin.h"

namespace AFS_Core {

using std::wstring;
using std::string;
using std::make_pair;
using util::toString;
using util::string_to_wstring;
using util::to_uint16_t;
namespace pt = boost::property_tree;

std::unique_ptr<Config> Config::instance_;
std::once_flag Config::initInstanceFlag_;

boost::property_tree::wptree parseJsonFile(wchar_t const* filename)
{
    boost::property_tree::wptree pt;
    if (!fsu::fileExists(filename)) {
#ifdef _WXWIDGETS
        wxMessageBox(wxString("Konfigurationsdatei existiert nicht: ") + wxString(filename));
#endif
        LOG_FATAL2(L"Konfigurationsdatei existiert nicht: ", filename)
            return pt;
    }
    try {
        auto in = std::wifstream(util::wstring_to_string(filename));
        boost::property_tree::read_json(in, pt);
    }
    catch (std::exception& ex) {
#ifdef _WXWIDGETS
        wxMessageBox(wxString("Ausnahme beim Lesen der Konfiguration: ") + ex.what());
#endif
        LOG_FATAL3(L"Ausnahme beim Lesen der Konfiguration: ", util::string_to_wstring(ex.what()), filename)
    }
    return pt;
}


SyncFlags::SyncFlags(bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled)
    : delAftercopy_(delAftercopy)
    , twoWaySync_(tws)
    , copyWhenTargetNewer_(cwtn)
    , fatherSonPrinciple_(fsp)
    , enabled_(enabled)
{}

//## class config ####################################################################################################
inline
void Config::initSingleton()
{
    instance_.reset(new Config);
}

Config& Config::instance()
{
    std::call_once(initInstanceFlag_, &Config::initSingleton);
    if (!instance_) {
        instance_.reset(new Config);
    }
    return *instance_;
}

bool Config::exists()
{
    return instance_ != nullptr;
}

bool Config::syncPossible()
{
    return exists() && instance().ready() && !instance().syncitems().empty();
}

namespace {

#if defined(_WIN32)
wstring legacyRootPathname()
{
    wchar_t* upstr = nullptr;
    size_t len = 0;
    _wdupenv_s(&upstr, &len, L"USERPROFILE");
    if (!upstr)
        return L"";
    wstring retval = wstring(upstr) + L"\\autofilesync.json";
    free(upstr);
    return retval;
}

wstring platformRootPathname()
{
    wchar_t* appData = nullptr;
    size_t len = 0;
    _wdupenv_s(&appData, &len, L"APPDATA");
    if (!appData)
        return L"autofilesync.json";
    wstring retval = wstring(appData) + L"\\autofilesync\\autofilesync.json";
    free(appData);
    return retval;
}
#elif defined(_MACOS)
wstring legacyRootPathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/autofilesync.json") : L"";
}

wstring platformRootPathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/Library/Application Support/autofilesync/autofilesync.json") : L"autofilesync.json";
}
#else // Linux
wstring legacyRootPathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/autofilesync.json") : L"";
}

wstring platformRootPathname()
{
    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    std::string base = xdgConfig && *xdgConfig ? xdgConfig : "";
    if (base.empty()) {
        const char* home = getenv("HOME");
        if (home)
            base = std::string(home) + "/.config";
    }
    return base.empty() ? L"autofilesync.json" : util::string_to_wstring(base + "/autofilesync/autofilesync.json");
}
#endif

// One-time move so users keep their settings after the config directory changed.
void migrateLegacyConfig(const wstring& newPath)
{
    const wstring oldPath = legacyRootPathname();
    if (oldPath.empty() || oldPath == newPath || !fsu::fileExists(oldPath) || fsu::fileExists(newPath))
        return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(newPath).parent_path(), ec);
    if (util::rename_file(oldPath, newPath))
        LOG_INFO3(L"Konfiguration migriert von: ", oldPath, newPath)
    else
        LOG_ERROR3(L"Migration der Konfiguration fehlgeschlagen von: ", oldPath, newPath)
}

std::filesystem::path passwordKeyFilePath()
{
    return std::filesystem::path(Config::getRootPathname()).parent_path() / L"secret.key";
}

} // namespace

wstring Config::getRootPathname()
{
    wstring retval = platformRootPathname();
    migrateLegacyConfig(retval);
    return retval;
}

void Config::reset()
{
    settings_.reset();
    {
        std::lock_guard<std::mutex> lock(syncitemsMutex_);
        syncitems_.clear();
    }
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    sshAccounts_.clear();
}

wstring getline(std::wfstream& in)
{
    wstring retval;
    do {
        std::getline(in, retval);
        if (retval.length() > 1 && retval.substr(0, 2) == L"//")
            retval = L"";  // remove comment
        if (!in)
            break;
    } while (retval.empty());
    return retval;
}

CompareItems getCompareItems(boost::property_tree::wptree& ci)
{
    CompareItems retval;
    retval.content(false);
    for (const auto& i: ci) {
        const wstring name = i.first;
        const wstring value=i.second.data();
        retval.set(name, value == L"true");
    }
    return retval;
}

Exclusions getExclusions(boost::property_tree::wptree& ci)
{
    // Mirrors writeSyncItem()'s exclusionsNode: a JSON array of plain strings,
    // each pushed with an empty key (i.first) and the pattern itself as the
    // node's own data (i.second.data()).
    Exclusions retval;
    for (const auto& i : ci) {
        const wstring pattern = i.second.data();
        if (!pattern.empty())
            retval.push_back(pattern);
    }
    return retval;
}

SyncFlags getSyncFlags(boost::property_tree::wptree& sf)
{
    SyncFlags retval;
    retval.delAftercopy(sf.get<wstring>(L"delAftercopy") == L"true");
    retval.twoWaySync(sf.get<wstring>(L"twoWaySync") == L"true");
    retval.copyWhenTargetNewer(sf.get<wstring>(L"copyWhenTargetNewer") == L"true");
    retval.fatherSonPrinciple(sf.get<wstring>(L"fatherSonPrinciple") == L"true");
    retval.deleteInTarget(sf.get<wstring>(L"syncDeletions", L"false") == L"true");
    // Default instead of the throwing get<>(): config files written before this
    // flag existed must keep loading, and off is the documented default.
    retval.backupProof(sf.get<wstring>(L"backupProof", L"false") == L"true");
    // Ebenfalls mit Vorgabe: aeltere Konfigurationsdateien kennen den Schluessel
    // nicht, und aus zu sein ist das dokumentierte Standardverhalten.
    retval.syncEmptyFiles(sf.get<wstring>(L"syncEmptyFiles", L"false") == L"true");
    retval.enabled(sf.get<wstring>(L"enabled") == L"true");
    //LOG_DEBUG2(L"enabled: ", sf.get<wstring>(L"enabled"))
    return retval;
}

Settings getSettings(boost::property_tree::wptree& sf)
{
    Settings retval;
    retval.startMinimized(sf.get<wstring>(L"StartMinimzed") == L"true");
    retval.minimizeInsteadOfClose(sf.get<wstring>(L"MinimizeInsteadOfClose") == L"true");
    retval.autoStart(sf.get<wstring>(L"Autostart") == L"true");
    retval.startSyncAtStart(sf.get<wstring>(L"StartSyncAtStart") == L"true");
    retval.showErrors(sf.get<wstring>(L"ShowErrors") == L"true");
    retval.showWarnings(sf.get<wstring>(L"ShowWarnings") == L"true");
    retval.showInfos(sf.get<wstring>(L"ShowInfos") == L"true");
    // Default instead of the throwing get<>() above: a config file written
    // before the delete mode existed must keep loading, and the safer
    // recycle bin mode is what it then falls back to.
    retval.deleteMode(sf.get<wstring>(L"DeleteMode", L"") == L"Permanent" ? DeleteMode::Permanent : DeleteMode::RecycleBin);
    // Same reason for the default as DeleteMode above: unset in older config
    // files, and empty is what "no proofs are written" already means.
    retval.backupProofPath(sf.get<wstring>(L"BackupProofPath", L""));
    return retval;
}

void Config::writeSettings(boost::property_tree::wptree& root)
{
    pt::wptree settings;
    settings.put(wstring(L"StartMinimzed"), toString(settings_.startMinimized()));
    settings.put(wstring(L"MinimizeInsteadOfClose"), toString(settings_.minimizeInsteadOfClose()));
    settings.put(wstring(L"Autostart"), toString(settings_.autoStart()));
    settings.put(wstring(L"StartSyncAtStart"), toString(settings_.startSyncAtStart()));
    settings.put(wstring(L"ShowErrors"), toString(settings_.showErrors()));
    settings.put(wstring(L"ShowWarnings"), toString(settings_.showWarnings()));
    settings.put(wstring(L"ShowInfos"), toString(settings_.showInfos()));
    settings.put(wstring(L"DeleteMode"), wstring(settings_.deleteMode() == DeleteMode::Permanent ? L"Permanent" : L"RecycleBin"));
    settings.put(wstring(L"RecycleBinPath"), RecycleBin::instance().binPath().wstring());
    settings.put(wstring(L"BackupProofPath"), settings_.backupProofPath());
    root.push_back(make_pair(wstring(L"Settings"), settings));
}

SshAccount getAccount(const std::wstring& path)
{
    auto colonPos = path.find(L':'); // Accountname steht vor dem ":" im Pfad
    if (colonPos == std::wstring::npos || colonPos == 0) {
        return SshAccount();
    }
    std::wstring accountName = path.substr(0, colonPos);
    auto found = Config::instance().findSshAccountByName(util::wstring_to_string(accountName));
    return found.value_or(SshAccount());
}

bool interpretSyncItem(boost::property_tree::wptree syncitemNode, SyncItem& si)
{
    try {
        auto compareitems = syncitemNode.get_child_optional(L"compareitems");
        if (compareitems != boost::none) {
            si.compareItems(getCompareItems(compareitems.get()));
        }
        auto exclusions = syncitemNode.get_child_optional(L"exclusions");
        if (exclusions != boost::none) {
            si.exclusions(getExclusions(exclusions.get()));
        }
        auto syncflags = syncitemNode.get_child_optional(L"syncflags");
        if (syncflags != boost::none) {
            auto flags = getSyncFlags(syncflags.get());
            si.flags(flags);
        }
        wstring name = syncitemNode.get<wstring>(L"name");
        wstring type = syncitemNode.get<wstring>(L"type");
        wstring source = syncitemNode.get<wstring>(L"source");
        wstring target = syncitemNode.get<wstring>(L"target");
        wstring wildcard = syncitemNode.get<wstring>(L"wildcard");
        wstring sourceType = syncitemNode.get<wstring>(L"sourceType", L"filesystem"); // Read source path with type
        if (sourceType == L"ssh") {
            si.setSourcePath(std::make_unique<SshPath>(getAccount(source), source));
        } else {
            si.setSourcePath(std::make_unique<FilesystemPath>(source));
        }
        wstring targetType = syncitemNode.get<wstring>(L"targetType", L"filesystem"); // Read target path with type
        if (targetType == L"ssh") {
            si.setTargetPath(std::make_unique<SshPath>(getAccount(target), target));
        } else {
            si.setTargetPath(std::make_unique<FilesystemPath>(target));
        }
        si.name(name);
        si.syncItemType(str2SyncItemType(type));
        si.wildcard(wildcard);
        si.syncCycle(to_uint16_t(syncitemNode.get<wstring>(L"SyncCycle")));
    } catch (std::exception& ex) {
        LOG_ERROR2("Lese Sync-Einstellungen: ", ex.what())
        return false;
    }
    return true;
}

// Same rules as readSshAccounts(): a value without the encryption marker is
// genuine legacy plaintext, while a failed decryption must not be handed on -
// the ciphertext blob would end up being sent as the password.
static wstring readStoredPassword(const wstring& stored)
{
    const string value = util::wstring_to_string(stored);
    if (!passwordcrypto::isEncrypted(value))
        return stored;
    if (auto decrypted = passwordcrypto::decrypt(value, passwordKeyFilePath()))
        return string_to_wstring(*decrypted);
    LOG_ERROR("SMTP-Passwort konnte nicht entschluesselt werden")
    return {};
}

static SmtpAccount readSmtpAccount(const boost::property_tree::wptree& logging)
{
    SmtpAccount smtp;
    smtp.server = logging.get<wstring>(L"SmtpServer", L"");
    smtp.port = to_uint16_t(logging.get<wstring>(L"SmtpPort", L"587"));
    smtp.user = logging.get<wstring>(L"SmtpUser", L"");
    smtp.password = readStoredPassword(logging.get<wstring>(L"SmtpPassword", L""));
    smtp.from = logging.get<wstring>(L"SmtpFrom", L"");
    return smtp;
}

bool Config::readLoggingSetup(boost::property_tree::wptree& root)
{
    try {
        auto logging = root.get_child_optional(L"Logging");
        if (logging != boost::none) {
            wstring LogLevel = logging->get<wstring>(L"LogLevel");
            Logging& log = Logging::instance();
            log.setLogLevel(log.getFromString(LogLevel));
            wstring LogFilePathName = logging->get<wstring>(L"LogFilePathName");
            log.setLogFilePathName(LogFilePathName);
            log.retentionDays(to_uint16_t(logging->get<wstring>(L"RetentionDays")));
            log.setEditorPathName(logging->get<wstring>(L"EditorPathName"));
            // Defaulted reads: the notification keys did not exist in older
            // configurations, and the throwing get<> above would have taken the
            // whole Logging section down with them.
            log.notifyEmailActive(logging->get<wstring>(L"NotifyEmailActive", L"false") == L"true");
            log.notifyEmail(logging->get<wstring>(L"NotifyEmail", L""));
            log.smtpAccount(readSmtpAccount(*logging));
        }
    }
    catch (std::exception& ex) {
        LOG_ERROR2("Lese Logging-Einstellungen: ", ex.what())
        return false;
    }
    return true;
}

bool Config::readSettings(boost::property_tree::wptree& root)
{
    try {
        auto settings = root.get_child_optional(L"Settings");
        if (settings != boost::none) {
            Settings s = getSettings(settings.value());
            settings_ = s;
            // Read with a default, unlike the fields in getSettings(): a config
            // file written before the recycle bin existed has no such key, and
            // the throwing get<>() would discard every other setting with it.
            const wstring recycleBinPath = settings->get<wstring>(L"RecycleBinPath", L"");
            if (!recycleBinPath.empty())
                RecycleBin::instance().binPath(recycleBinPath);
        }
    }
    catch (std::exception& ex) {
        #ifdef _WXWIDGETS
            wxMessageBox(wxString("read settings: ") + ex.what());
        #endif
        LOG_ERROR2("Fehler beim Lesen der Einstellungen: ", ex.what())
        return false;
    }
    return true;
}

bool Config::readSshAccounts(boost::property_tree::wptree& root)
{
    auto sshAccountsNode = root.get_child_optional(L"SshAccounts");
    if (sshAccountsNode == boost::none)
        return true;
    bool allOk = true;
    for (auto iter = sshAccountsNode.value().begin(); iter != sshAccountsNode.value().end(); iter++) {
        // Caught per account, not around the whole loop: one malformed entry
        // must not discard every other account - see interpretSyncItem()'s
        // per-item handling below for the same reasoning.
        try {
            SshAccount account;
            account.setName(util::wstring_to_string(iter->second.get<wstring>(L"name")));
            account.setHost(util::wstring_to_string(iter->second.get<wstring>(L"host")));
            account.setUser(util::wstring_to_string(iter->second.get<wstring>(L"user")));
            const std::string storedPassword = util::wstring_to_string(iter->second.get<wstring>(L"password"));
            if (!passwordcrypto::isEncrypted(storedPassword)) {
                account.setPassword(storedPassword); // genuine legacy plaintext, predates encryption
            } else if (auto decrypted = passwordcrypto::decrypt(storedPassword, passwordKeyFilePath())) {
                account.setPassword(*decrypted);
            } else {
                // Do NOT fall back to storedPassword here: it is the raw
                // "$AES256GCM$..." ciphertext blob, not a usable password -
                // using it as one would submit garbage to the SSH server with
                // no indication of what actually went wrong (lost/changed key).
                LOG_FATAL2("Passwort konnte nicht entschluesselt werden fuer SSH-Account: ", account.name())
            }
            add(account, true);
        }
        catch (std::exception& ex) {
            LOG_ERROR2("Fehlerhafter SSH-Account-Eintrag wird uebersprungen: ", ex.what())
            allOk = false;
        }
    }
    return allOk;
}

bool Config::interpretJson(boost::property_tree::wptree&& root)
{
    if (root.empty()) {
        LOG_FATAL("Fehler beim Lesen der Konfiguration.")
        return false;
    }
    readSettings(root);
    readLoggingSetup(root);
    readSshAccounts(root);
    auto syncitems = root.get_child_optional(L"Syncitems");
    if (syncitems == boost::none)
        return true;
    for (auto iter = syncitems.value().begin(); iter != syncitems.value().end(); iter++) {
        SyncItem si;
        if (interpretSyncItem(iter->second, si))
            add(si, true);
        else
            LOG_ERROR(L"Fehlerhafter Syncitem-Eintrag wird übersprungen.")
    }
    return true;
}

void Config::writeLoggingSetup(boost::property_tree::wptree& root)
{
	pt::wptree logging;
    Logging& log = Logging::instance();
    wstring LogLevel = string_to_wstring(log.logLevelStr(log.getLogLevel()));
	logging.put(wstring(L"LogLevel"), LogLevel);
	logging.put(wstring(L"LogFilePathName"), log.getLogFilePathName());
    logging.put(wstring(L"RetentionDays"), toString(log.retentionDays()));
    logging.put(wstring(L"EditorPathName"), log.getEditorPathName());
    logging.put(wstring(L"NotifyEmailActive"), toString(log.notifyEmailActive()));
    logging.put(wstring(L"NotifyEmail"), log.notifyEmail());
    const SmtpAccount smtp = log.smtpAccount();
    logging.put(wstring(L"SmtpServer"), smtp.server);
    logging.put(wstring(L"SmtpPort"), toString(smtp.port));
    logging.put(wstring(L"SmtpUser"), smtp.user);
    logging.put(wstring(L"SmtpFrom"), smtp.from);
    // Encrypted like the SSH passwords; an empty password stays empty so that
    // readStoredPassword() does not have to tell "unset" from "undecryptable".
    logging.put(wstring(L"SmtpPassword"), smtp.password.empty() ? wstring()
        : string_to_wstring(passwordcrypto::encrypt(util::wstring_to_string(smtp.password), passwordKeyFilePath())));
	root.push_back(make_pair(wstring(L"Logging"), logging));
}

void Config::writeSshAccounts(boost::property_tree::wptree& root)
{
    pt::wptree sshAccountsNode;
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    for (const auto& account : sshAccounts_) {
        pt::wptree accountNode;
        accountNode.put(wstring(L"name"), string_to_wstring(account.name()));
        accountNode.put(wstring(L"host"), string_to_wstring(account.host()));
        accountNode.put(wstring(L"user"), string_to_wstring(account.user()));
        accountNode.put(wstring(L"password"), string_to_wstring(passwordcrypto::encrypt(account.password(), passwordKeyFilePath())));
        sshAccountsNode.push_back(make_pair(wstring(L"SshAccount"), accountNode));
    }
    root.push_back(make_pair(wstring(L"SshAccounts"), sshAccountsNode));
}

void writeCompareItems(const SyncItem& inode, pt::wptree& compareItemsNode)
{
    CompareItems ci = inode.compareItems();
    pt::wptree sizeNode;
    sizeNode.put(L"", toString(ci.size()));
    compareItemsNode.push_back(std::make_pair(L"size", sizeNode));
    pt::wptree timeNode;
    timeNode.put(L"", toString(ci.time()));
    compareItemsNode.push_back(std::make_pair(L"time", timeNode));
    pt::wptree contentNode;
    contentNode.put(L"", toString(ci.content()));
    compareItemsNode.push_back(std::make_pair(L"content", contentNode));
}

void Config::writeSyncItem(const SyncItem& inode, boost::property_tree::wptree& syncitemNode)
{
    pt::wptree syncFlagsNode;
    syncFlagsNode.put(wstring(L"delAftercopy"), toString(inode.delAftercopy()));
    syncFlagsNode.put(wstring(L"twoWaySync"), toString(inode.twoWaySync()));
    syncFlagsNode.put(wstring(L"copyWhenTargetNewer"), toString(inode.copyWhenTargetNewer()));
    syncFlagsNode.put(wstring(L"fatherSonPrinciple"), toString(inode.fatherSonPrinciple()));
    syncFlagsNode.put(wstring(L"syncDeletions"), toString(inode.deleteInTarget()));
    syncFlagsNode.put(wstring(L"backupProof"), toString(inode.backupProof()));
    syncFlagsNode.put(wstring(L"syncEmptyFiles"), toString(inode.syncEmptyFiles()));
    syncFlagsNode.put(wstring(L"enabled"), toString(inode.enabled()));
    pt::wptree compareItemsNode;
    writeCompareItems(inode, compareItemsNode);
    pt::wptree exclusionsNode;
    for (const auto& ci : inode.exclusions()) {
        pt::wptree exclusionItemNode;
        exclusionItemNode.put(L"", ci);
        exclusionsNode.push_back(std::make_pair(L"", exclusionItemNode));
    }
    syncitemNode.push_back(make_pair(wstring(L"compareitems"), compareItemsNode));
    syncitemNode.push_back(make_pair(wstring(L"exclusions"), exclusionsNode));
    syncitemNode.push_back(make_pair(wstring(L"syncflags"), syncFlagsNode));
    syncitemNode.put(wstring(L"name"), inode.name());
    // The untranslated key, not the displayed text: a configuration written
    // with one language must stay readable with any other.
    syncitemNode.put(wstring(L"type"), syncItemTypeKey(inode.syncItemType()));
    
    // Write source path
    if (inode.sourcePath()) {
        syncitemNode.put(wstring(L"source"), inode.source());
        syncitemNode.put(wstring(L"sourceType"), inode.sourcePath()->type() == SyncPathType::Filesystem ? L"filesystem" : L"ssh");
    }
    
    // Write target path
    if (inode.targetPath()) {
        syncitemNode.put(wstring(L"target"), inode.target());
        syncitemNode.put(wstring(L"targetType"), inode.targetPath()->type() == SyncPathType::Filesystem ? L"filesystem" : L"ssh");
    }
    
    syncitemNode.put(wstring(L"wildcard"), inode.wildcard());
    syncitemNode.put(wstring(L"SyncCycle"), toString(inode.syncCycle()));
}

void Config::writeSyncItems(boost::property_tree::wptree& root)
{
    pt::wptree syncitemsNode;
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (const auto& inode : syncitems_)
    {
        pt::wptree syncitemNode;
        writeSyncItem(inode, syncitemNode);
        syncitemsNode.push_back(make_pair(wstring(L"Syncitem"), syncitemNode));
    }
    root.push_back(make_pair(wstring(L"Syncitems"), syncitemsNode));
}

bool Config::write(const std::wstring& pathname)
{
    pt::wptree root;
    writeSettings(root);
    writeLoggingSetup(root);
    writeSshAccounts(root);
    writeSyncItems(root);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(pathname).parent_path(), ec);
    const wstring tmpPathname = pathname + L".tmp";
    {
        std::wfstream os(util::wstring_to_string(tmpPathname), std::ios_base::out);
        try {
            os.exceptions(std::ios::failbit | std::ios::badbit);
            pt::write_json(os, root);
        }
        catch (std::exception& ex) {
#ifdef _WXWIDGETS
            wxMessageBox(wxString("Fehler beim Schreiben der Konfiguration: ") + ex.what());
#endif
            LOG_FATAL2("Fehler beim Schreiben der Konfiguration: ", ex.what());
            util::remove_file(tmpPathname);
            return false;
        }
    }
    if (!util::rename_file(tmpPathname, pathname))
        return false;
    changed_ = false;
    return true;
}

bool Config::read(std::wstring_view pathname)
{
    reset();
    ready_ = false;
    if (!interpretJson(parseJsonFile(pathname.data()))) {
        reset();
#ifdef _WXWIDGETS
        wxMessageBox(wxString("Konfiguration konnte nicht gelesen werden."));
#endif
        LOG_FATAL("Konfiguration konnte nicht gelesen werden.");
        return false;
    }
    ready_ = true;
    changed_= false;
    return true;
}

void Config::add(const SyncItem& si, bool loading)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    syncitems_.push_back(si);
    if (!loading)
        changed_ = true;
}

void Config::add(const SshAccount& account, bool loading)
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    sshAccounts_.push_back(account);
    if (!loading)
        changed_ = true;
}

std::optional<SshAccount> Config::findSshAccountByName(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    for (const auto& acc : sshAccounts_) {
        if (acc.name() == name)
            return acc;
    }
    return std::nullopt;
}

std::optional<SshAccount> Config::sshAccountAt(std::size_t index) const
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    if (index >= sshAccounts_.size())
        return std::nullopt;
    return sshAccounts_[index];
}

SshAccounts Config::sshAccountsSnapshot() const
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    return sshAccounts_;
}

bool Config::removeSshAccount(std::size_t index)
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    if (index >= sshAccounts_.size())
        return false;
    sshAccounts_.erase(sshAccounts_.begin() + index);
    changed_ = true;
    return true;
}

bool Config::updateSshAccount(std::size_t index, const SshAccount& updated)
{
    std::lock_guard<std::mutex> lock(sshAccountsMutex_);
    if (index >= sshAccounts_.size())
        return false;
    sshAccounts_[index] = updated;
    changed_ = true;
    return true;
}

void Config::set(unsigned i, const SyncItem& si)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    syncitems_[i]=si;
    changed_ = true;
}

void Config::remove(unsigned i)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    auto iter=syncitems_.begin();
    std::advance(iter, i);
    syncitems_.erase(iter);
    changed_ = true;
}

bool Config::eraseByName(const std::wstring& name)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (auto iter = syncitems_.begin(); iter != syncitems_.end(); ++iter) {
        if (iter->name() == name) {
            syncitems_.erase(iter);
            changed_ = true;
            return true;
        }
    }
    return false;
}

void Config::pathname(std::wstring_view p)
{
    pathname_=p;
}

SyncItem Config::item(unsigned i) const
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    return syncitems_[i];
}

SyncItems Config::syncitems() const
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    return syncitems_;
}

void Config::refreshLastSync(const std::wstring& name)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (SyncItem& i : syncitems_) {
        if (i.name() == name) {
            i.setLastSync();
            return;
        }
    }
}

std::optional<SyncItem> Config::find(const std::wstring& path, bool source) const
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (const SyncItem& i: syncitems_) {
        if ((source && i.source() == path) || (!source && i.target() == path))
            return i;
    }
    return std::nullopt;
}

std::optional<SyncItem> Config::findByName(const std::wstring& name) const
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (const SyncItem& i : syncitems_) {
        if (i.name() == name)
            return i;
    }
    return std::nullopt;
}

bool Config::updateByName(const std::wstring& name, const SyncItem& updated)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (SyncItem& i : syncitems_) {
        if (i.name() == name) {
            i = updated;
            changed_ = true;
            return true;
        }
    }
    return false;
}

bool Config::updateSshAccountReferences(const std::string& oldName, const SshAccount& newAccount)
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    bool any = false;
    for (SyncItem& i : syncitems_)
        if (i.renameSshAccountReference(oldName, newAccount))
            any = true;
    if (any)
        changed_ = true;
    return any;
}

bool Config::sshAccountReferencedByAnyItem(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(syncitemsMutex_);
    for (const SyncItem& i : syncitems_)
        if (i.referencesSshAccount(name))
            return true;
    return false;
}

void Config::unload()
{
	if (!instance_)
		return;
	instance_->reset();  // Clear data
	instance_.reset();   // Delete instance to avoid false positive memory leak reports
}

} // namespace AFS_Core
