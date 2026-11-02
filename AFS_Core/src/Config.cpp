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
#include "pch.h"
#include "Config.h"
#include <algorithm>
#include "SyncState.h"
#include "JsonText.h"
#include "Util.h"
#include "PasswordCrypto.h"
#include <fstream>
#include <sstream>
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
#include "Translation.h"

namespace AFS_Core {

using std::wstring;
using std::string;
using std::make_pair;
using util::to_string;
using util::string_to_wstring;
using util::to_uint16_t;
namespace pt = boost::property_tree;

std::unique_ptr<Config> Config::instance_;
std::once_flag Config::init_instance_flag_;

boost::property_tree::wptree parse_json_file(wchar_t const* filename)
{
    boost::property_tree::wptree pt;
    if (!fsu::file_exists(filename)) {
#ifdef _WXWIDGETS
        wxMessageBox(wxString(translation::translate("config_file_not_exist")) + wxString(filename));
#endif
        log_fatal(translation::translate("config_file_not_exist"), filename);
            return pt;
    }
    try {
        std::wistringstream in(util::read_utf8_file(filename));  // [Config.md#utf8-file]
        boost::property_tree::read_json(in, pt);
    }
    catch (std::exception& ex) {
#ifdef _WXWIDGETS
        wxMessageBox(wxString(translation::translate("config_read_exception")) + ex.what());
#endif
        log_fatal(translation::translate("config_read_exception"), util::string_to_wstring(ex.what()), filename);
    }
    return pt;
}

inline
void Config::init_singleton()
{
    instance_.reset(new Config);
}

Config& Config::instance()
{
    std::call_once(init_instance_flag_, &Config::init_singleton);
    if (!instance_) {
        instance_.reset(new Config);
    }
    return *instance_;
}

bool Config::exists()
{
    return instance_ != nullptr;
}

bool Config::sync_possible()
{
    return exists() && instance().ready() && !instance().syncitems().empty();
}

namespace {

#if defined(_WIN32)
wstring legacy_root_pathname()
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

wstring platform_root_pathname()
{
    wchar_t* app_data = nullptr;
    size_t len = 0;
    _wdupenv_s(&app_data, &len, L"APPDATA");
    if (!app_data)
        return L"autofilesync.json";
    wstring retval = wstring(app_data) + L"\\autofilesync\\autofilesync.json";
    free(app_data);
    return retval;
}
#elif defined(_MACOS)
wstring legacy_root_pathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/autofilesync.json") : L"";
}

wstring platform_root_pathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/Library/Application Support/autofilesync/autofilesync.json") : L"autofilesync.json";
}
#else // Linux
wstring legacy_root_pathname()
{
    const char* home = getenv("HOME");
    return home ? util::string_to_wstring(std::string(home) + "/autofilesync.json") : L"";
}

wstring platform_root_pathname()
{
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    std::string base = xdg_config && *xdg_config ? xdg_config : "";
    if (base.empty()) {
        const char* home = getenv("HOME");
        if (home)
            base = std::string(home) + "/.config";
    }
    return base.empty() ? L"autofilesync.json" : util::string_to_wstring(base + "/autofilesync/autofilesync.json");
}
#endif

void migrate_legacy_config(const wstring& new_path)  // [Config.md#migrate_legacy_config]
{
    const wstring old_path = legacy_root_pathname();
    if (old_path.empty() || old_path == new_path || !fsu::file_exists(old_path) || fsu::file_exists(new_path))
        return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(new_path).parent_path(), ec);
    if (util::rename_file(old_path, new_path))
        log_info(translation::translate("config_migrated_from"), old_path, new_path);
    else
        log_error(translation::translate("config_migration_failed"), old_path, new_path);
}

std::filesystem::path password_key_file_path()
{
    return std::filesystem::path(Config::get_root_pathname()).parent_path() / L"secret.key";
}

} // namespace

wstring Config::get_root_pathname()
{
    wstring retval = platform_root_pathname();
    migrate_legacy_config(retval);
    return retval;
}

void Config::reset()
{
    settings_.reset();
    {
        std::lock_guard<std::mutex> lock(syncitems_mutex_);
        syncitems_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
        ssh_accounts_.clear();
    }
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.clear();
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

CompareItems get_compare_items(boost::property_tree::wptree& ci)
{
    CompareItems retval;
    retval.content(false);
    for (const auto& i: ci) {
        const wstring name = i.first;
        const wstring value=i.second.data();
        retval.set(name, value == L"true");
    }
    retval.time_tolerance_seconds(to_uint16_t(ci.get<wstring>(L"timeToleranceSeconds", std::to_wstring(DEFAULT_TIME_TOLERANCE_SECONDS))));  // [Config.md#defaulted-flag-reads]
    return retval;
}

Exclusions get_exclusions(boost::property_tree::wptree& ci)
{
    Exclusions retval;  // [Config.md#exclusions-node-format]
    for (const auto& i : ci) {
        const wstring pattern = i.second.data();
        if (!pattern.empty())
            retval.push_back(pattern);
    }
    return retval;
}

SyncFlags get_sync_flags(boost::property_tree::wptree& sf)
{
    SyncFlags retval;
    retval.del_aftercopy(sf.get<wstring>(L"delAftercopy") == L"true");
    retval.two_way_sync(sf.get<wstring>(L"twoWaySync") == L"true");
    retval.copy_when_target_newer(sf.get<wstring>(L"copyWhenTargetNewer") == L"true");
    retval.father_son_principle(sf.get<wstring>(L"fatherSonPrinciple") == L"true");
    retval.delete_in_target(sf.get<wstring>(L"syncDeletions", L"false") == L"true");
    retval.backup_proof(sf.get<wstring>(L"backupProof", L"false") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.sync_empty_files(sf.get<wstring>(L"syncEmptyFiles", L"false") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.warn_if_unreachable(sf.get<wstring>(L"warnIfUnreachable", L"true") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.track_moves(sf.get<wstring>(L"trackMoves", L"false") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.conflict_copies(sf.get<wstring>(L"conflictCopies", L"true") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.watch_changes(sf.get<wstring>(L"watchChanges", L"false") == L"true");  // [Config.md#defaulted-flag-reads]
    retval.enabled(sf.get<wstring>(L"enabled") == L"true");
    return retval;
}

Settings get_settings(boost::property_tree::wptree& sf)
{
    Settings retval;
    retval.start_minimized(sf.get<wstring>(L"StartMinimzed") == L"true");
    retval.minimize_instead_of_close(sf.get<wstring>(L"MinimizeInsteadOfClose") == L"true");
    retval.auto_start(sf.get<wstring>(L"Autostart") == L"true");
    retval.start_sync_at_start(sf.get<wstring>(L"StartSyncAtStart") == L"true");
    retval.show_errors(sf.get<wstring>(L"ShowErrors", L"true") == L"true");  // [Config.md#delete-mode-default]
    retval.show_warnings(sf.get<wstring>(L"ShowWarnings", L"false") == L"true");
    retval.show_infos(sf.get<wstring>(L"ShowInfos", L"false") == L"true");
    retval.delete_mode(sf.get<wstring>(L"DeleteMode", L"") == L"Permanent" ? DeleteMode::Permanent : DeleteMode::RecycleBin);  // [Config.md#delete-mode-default]
    retval.backup_proof_path(sf.get<wstring>(L"BackupProofPath", L""));  // [Config.md#delete-mode-default]
    retval.language(util::wstring_to_string(sf.get<wstring>(L"Language", L"")));
    return retval;
}

void Config::write_settings(boost::property_tree::wptree& root)
{
    pt::wptree settings;
    settings.put(wstring(L"StartMinimzed"), to_string(settings_.start_minimized()));
    settings.put(wstring(L"MinimizeInsteadOfClose"), to_string(settings_.minimize_instead_of_close()));
    settings.put(wstring(L"Autostart"), to_string(settings_.auto_start()));
    settings.put(wstring(L"StartSyncAtStart"), to_string(settings_.start_sync_at_start()));
    settings.put(wstring(L"ShowErrors"), to_string(settings_.show_errors()));
    settings.put(wstring(L"ShowWarnings"), to_string(settings_.show_warnings()));
    settings.put(wstring(L"ShowInfos"), to_string(settings_.show_infos()));
    settings.put(wstring(L"DeleteMode"), wstring(settings_.delete_mode() == DeleteMode::Permanent ? L"Permanent" : L"RecycleBin"));
    settings.put(wstring(L"RecycleBinPath"), RecycleBin::instance().bin_path().wstring());
    settings.put(wstring(L"BackupProofPath"), settings_.backup_proof_path());
    settings.put(wstring(L"Language"), util::string_to_wstring(settings_.language()));
    root.push_back(make_pair(wstring(L"Settings"), settings));
}

SshAccount get_account(const std::wstring& path)
{
    auto colon_pos = path.find(L':'); // the account name precedes the ":"
    if (colon_pos == std::wstring::npos || colon_pos == 0) {
        return SshAccount();
    }
    std::wstring account_name = path.substr(0, colon_pos);
    auto found = Config::instance().find_ssh_account_by_name(util::wstring_to_string(account_name));
    return found.value_or(SshAccount());
}

bool interpret_sync_item(boost::property_tree::wptree syncitem_node, SyncItem& si)
{
    try {
        auto compareitems = syncitem_node.get_child_optional(L"compareitems");
        if (compareitems != boost::none) {
            si.compare_items(get_compare_items(compareitems.get()));
        }
        auto exclusions = syncitem_node.get_child_optional(L"exclusions");
        if (exclusions != boost::none) {
            si.exclusions(get_exclusions(exclusions.get()));
        }
        auto syncflags = syncitem_node.get_child_optional(L"syncflags");
        if (syncflags != boost::none) {
            auto flags = get_sync_flags(syncflags.get());
            si.flags(flags);
        }
        wstring name = syncitem_node.get<wstring>(L"name");
        wstring type = syncitem_node.get<wstring>(L"type");
        wstring source = syncitem_node.get<wstring>(L"source");
        wstring target = syncitem_node.get<wstring>(L"target");
        wstring wildcard = syncitem_node.get<wstring>(L"wildcard");
        wstring source_type = syncitem_node.get<wstring>(L"sourceType", L"filesystem"); // Read source path with type
        if (source_type == L"ssh") {
            si.set_source_path(std::make_unique<SshPath>(get_account(source), source));
        } else {
            si.set_source_path(std::make_unique<FilesystemPath>(source));
        }
        wstring target_type = syncitem_node.get<wstring>(L"targetType", L"filesystem"); // Read target path with type
        if (target_type == L"ssh") {
            si.set_target_path(std::make_unique<SshPath>(get_account(target), target));
        } else {
            si.set_target_path(std::make_unique<FilesystemPath>(target));
        }
        si.name(name);
        si.sync_item_type(str_to_sync_item_type(type));
        si.wildcard(wildcard);
        si.sync_cycle(to_uint16_t(syncitem_node.get<wstring>(L"SyncCycle")));
    } catch (std::exception& ex) {
        log_error(translation::translate_utf8("config_read_sync_settings"), ex.what());
        return false;
    }
    return true;
}

static wstring read_stored_password(const wstring& stored)  // [Config.md#read_stored_password]
{
    const string value = util::wstring_to_string(stored);
    if (!passwordcrypto::is_encrypted(value))
        return stored;
    if (auto decrypted = passwordcrypto::decrypt(value, password_key_file_path()))
        return string_to_wstring(*decrypted);
    log_error(translation::translate_utf8("smtp_password_not_decryptable"));
    return {};
}

static SmtpAccount read_smtp_account(const boost::property_tree::wptree& logging)
{
    SmtpAccount smtp;
    smtp.server = logging.get<wstring>(L"SmtpServer", L"");
    smtp.port = to_uint16_t(logging.get<wstring>(L"SmtpPort", L"587"));
    smtp.user = logging.get<wstring>(L"SmtpUser", L"");
    smtp.password = read_stored_password(logging.get<wstring>(L"SmtpPassword", L""));
    smtp.from = logging.get<wstring>(L"SmtpFrom", L"");
    return smtp;
}

bool Config::read_logging_setup(boost::property_tree::wptree& root)
{
    try {
        auto logging = root.get_child_optional(L"Logging");
        if (logging != boost::none) {
            wstring log_level = logging->get<wstring>(L"LogLevel");
            Logging& log = Logging::instance();
            log.set_log_level(log.get_from_string(log_level));
            wstring log_file_path_name = logging->get<wstring>(L"LogFilePathName");
            log.set_log_file_path_name(log_file_path_name);
            log.retention_days(to_uint16_t(logging->get<wstring>(L"RetentionDays")));
            log.set_editor_path_name(logging->get<wstring>(L"EditorPathName"));
            log.email().notify_active(logging->get<wstring>(L"NotifyEmailActive", L"false") == L"true");  // [Config.md#logging-notification-defaults]
            log.email().notify_address(logging->get<wstring>(L"NotifyEmail", L""));
            log.email().smtp_account(read_smtp_account(*logging));
        }
    }
    catch (std::exception& ex) {
        log_error(translation::translate_utf8("config_read_logging_settings"), ex.what());
        return false;
    }
    return true;
}

bool Config::read_settings(boost::property_tree::wptree& root)
{
    try {
        auto settings = root.get_child_optional(L"Settings");
        if (settings != boost::none) {
            Settings s = get_settings(settings.value());
            settings_ = s;
            const wstring recycle_bin_path = settings->get<wstring>(L"RecycleBinPath", L"");  // [Config.md#recycle-bin-path-default]
            if (!recycle_bin_path.empty())
                RecycleBin::instance().bin_path(recycle_bin_path);
        }
    }
    catch (std::exception& ex) {
        #ifdef _WXWIDGETS
            wxMessageBox(wxString(translation::translate("config_read_settings")) + ex.what());
        #endif
        log_error(translation::translate_utf8("config_settings_read_error"), ex.what());
        return false;
    }
    return true;
}

bool Config::read_ssh_accounts(boost::property_tree::wptree& root)
{
    auto ssh_accounts_node = root.get_child_optional(L"SshAccounts");
    if (ssh_accounts_node == boost::none)
        return true;
    bool all_ok = true;
    for (auto iter = ssh_accounts_node.value().begin(); iter != ssh_accounts_node.value().end(); iter++) {
        try {  // [Config.md#per-account-error-handling]
            SshAccount account;
            account.set_name(util::wstring_to_string(iter->second.get<wstring>(L"name")));
            account.set_host(util::wstring_to_string(iter->second.get<wstring>(L"host")));
            account.set_user(util::wstring_to_string(iter->second.get<wstring>(L"user")));
            account.set_port(static_cast<uint16_t>(iter->second.get<int>(L"port", 22)));  // [Config.md#ssh-port]
            account.set_host_key(util::wstring_to_string(iter->second.get<wstring>(L"hostKey", L"")));  // [Config.md#ssh-host-key]
            const std::string stored_password = util::wstring_to_string(iter->second.get<wstring>(L"password"));
            if (!passwordcrypto::is_encrypted(stored_password)) {
                account.set_password(stored_password); // genuine legacy plaintext, predates encryption
            } else if (auto decrypted = passwordcrypto::decrypt(stored_password, password_key_file_path())) {
                account.set_password(*decrypted);
            } else {
                log_fatal(translation::translate_utf8("ssh_password_not_decryptable"), account.name());  // [Config.md#no-ciphertext-fallback]
            }
            add(account, true);
        }
        catch (std::exception& ex) {
            log_error(translation::translate_utf8("ssh_account_entry_skipped"), ex.what());
            all_ok = false;
        }
    }
    return all_ok;
}

bool Config::interpret_json(boost::property_tree::wptree&& root)
{
    if (root.empty()) {
        log_fatal(translation::translate_utf8("config_read_error"));
        return false;
    }
    read_settings(root);
    read_logging_setup(root);
    read_ssh_accounts(root);
    read_devices(root);
    auto syncitems = root.get_child_optional(L"Syncitems");
    if (syncitems == boost::none)
        return true;
    for (auto iter = syncitems.value().begin(); iter != syncitems.value().end(); iter++) {
        SyncItem si;
        if (interpret_sync_item(iter->second, si))
            add(si, true);
        else
            log_error(translation::translate("sync_item_entry_skipped"));
    }
    return true;
}

void Config::write_logging_setup(boost::property_tree::wptree& root)
{
	pt::wptree logging;
    Logging& log = Logging::instance();
    wstring log_level = string_to_wstring(log.log_level_str(log.get_log_level()));
	logging.put(wstring(L"LogLevel"), log_level);
	logging.put(wstring(L"LogFilePathName"), log.get_log_file_path_name());
    logging.put(wstring(L"RetentionDays"), to_string(log.retention_days()));
    logging.put(wstring(L"EditorPathName"), log.get_editor_path_name());
    logging.put(wstring(L"NotifyEmailActive"), to_string(log.email().notify_active()));
    logging.put(wstring(L"NotifyEmail"), log.email().notify_address());
    const SmtpAccount smtp = log.email().smtp_account();
    logging.put(wstring(L"SmtpServer"), smtp.server);
    logging.put(wstring(L"SmtpPort"), to_string(smtp.port));
    logging.put(wstring(L"SmtpUser"), smtp.user);
    logging.put(wstring(L"SmtpFrom"), smtp.from);
    logging.put(wstring(L"SmtpPassword"), smtp.password.empty() ? wstring()  // [Config.md#smtp-password-encryption]
        : string_to_wstring(passwordcrypto::encrypt(util::wstring_to_string(smtp.password), password_key_file_path())));
	root.push_back(make_pair(wstring(L"Logging"), logging));
}

void Config::write_ssh_accounts(boost::property_tree::wptree& root)
{
    pt::wptree ssh_accounts_node;
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    for (const auto& account : ssh_accounts_) {
        pt::wptree account_node;
        account_node.put(wstring(L"name"), string_to_wstring(account.name()));
        account_node.put(wstring(L"host"), string_to_wstring(account.host()));
        account_node.put(wstring(L"user"), string_to_wstring(account.user()));
        account_node.put(wstring(L"port"), std::to_wstring(account.port()));
        account_node.put(wstring(L"hostKey"), string_to_wstring(account.host_key()));  // [Config.md#ssh-host-key]
        account_node.put(wstring(L"password"), string_to_wstring(passwordcrypto::encrypt(account.password(), password_key_file_path())));
        ssh_accounts_node.push_back(make_pair(wstring(L"SshAccount"), account_node));
    }
    root.push_back(make_pair(wstring(L"SshAccounts"), ssh_accounts_node));
}

void write_compare_items(const SyncItem& inode, pt::wptree& compare_items_node)
{
    CompareItems ci = inode.compare_items();
    pt::wptree size_node;
    size_node.put(L"", to_string(ci.size()));
    compare_items_node.push_back(std::make_pair(L"size", size_node));
    pt::wptree time_node;
    time_node.put(L"", to_string(ci.time()));
    compare_items_node.push_back(std::make_pair(L"time", time_node));
    pt::wptree content_node;
    content_node.put(L"", to_string(ci.content()));
    compare_items_node.push_back(std::make_pair(L"content", content_node));
    pt::wptree tolerance_node;
    tolerance_node.put(L"", std::to_wstring(ci.time_tolerance_seconds()));
    compare_items_node.push_back(std::make_pair(L"timeToleranceSeconds", tolerance_node));
}

void Config::write_sync_item(const SyncItem& inode, boost::property_tree::wptree& syncitem_node)
{
    pt::wptree sync_flags_node;
    sync_flags_node.put(wstring(L"delAftercopy"), to_string(inode.del_aftercopy()));
    sync_flags_node.put(wstring(L"twoWaySync"), to_string(inode.two_way_sync()));
    sync_flags_node.put(wstring(L"copyWhenTargetNewer"), to_string(inode.copy_when_target_newer()));
    sync_flags_node.put(wstring(L"fatherSonPrinciple"), to_string(inode.father_son_principle()));
    sync_flags_node.put(wstring(L"syncDeletions"), to_string(inode.delete_in_target()));
    sync_flags_node.put(wstring(L"backupProof"), to_string(inode.backup_proof()));
    sync_flags_node.put(wstring(L"syncEmptyFiles"), to_string(inode.sync_empty_files()));
    sync_flags_node.put(wstring(L"warnIfUnreachable"), to_string(inode.warn_if_unreachable()));
    sync_flags_node.put(wstring(L"trackMoves"), to_string(inode.track_moves()));
    sync_flags_node.put(wstring(L"conflictCopies"), to_string(inode.conflict_copies()));
    sync_flags_node.put(wstring(L"watchChanges"), to_string(inode.watch_changes()));
    sync_flags_node.put(wstring(L"enabled"), to_string(inode.enabled()));
    pt::wptree compare_items_node;
    write_compare_items(inode, compare_items_node);
    pt::wptree exclusions_node;
    for (const auto& ci : inode.exclusions()) {
        pt::wptree exclusion_item_node;
        exclusion_item_node.put(L"", ci);
        exclusions_node.push_back(std::make_pair(L"", exclusion_item_node));
    }
    syncitem_node.push_back(make_pair(wstring(L"compareitems"), compare_items_node));
    syncitem_node.push_back(make_pair(wstring(L"exclusions"), exclusions_node));
    syncitem_node.push_back(make_pair(wstring(L"syncflags"), sync_flags_node));
    syncitem_node.put(wstring(L"name"), inode.name());
    syncitem_node.put(wstring(L"type"), sync_item_type_key(inode.sync_item_type()));  // [Config.md#untranslated-type-key]

    if (inode.source_path()) {
        syncitem_node.put(wstring(L"source"), inode.source());
        syncitem_node.put(wstring(L"sourceType"), inode.source_path()->type() == SyncPathType::Filesystem ? L"filesystem" : L"ssh");
    }

    if (inode.target_path()) {
        syncitem_node.put(wstring(L"target"), inode.target());
        syncitem_node.put(wstring(L"targetType"), inode.target_path()->type() == SyncPathType::Filesystem ? L"filesystem" : L"ssh");
    }

    syncitem_node.put(wstring(L"wildcard"), inode.wildcard());
    syncitem_node.put(wstring(L"SyncCycle"), to_string(inode.sync_cycle()));
}

void Config::write_sync_items(boost::property_tree::wptree& root)
{
    pt::wptree syncitems_node;
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (const auto& inode : syncitems_)
    {
        pt::wptree syncitem_node;
        write_sync_item(inode, syncitem_node);
        syncitems_node.push_back(make_pair(wstring(L"Syncitem"), syncitem_node));
    }
    root.push_back(make_pair(wstring(L"Syncitems"), syncitems_node));
}

namespace {

std::filesystem::file_time_type disk_time(const std::wstring& pathname)
{
    std::error_code ec;
    return std::filesystem::last_write_time(pathname, ec);  // a missing file yields the default time, so its later appearance counts as a change
}

}

bool Config::write(const std::wstring& pathname)
{
    pt::wptree root;
    write_settings(root);
    write_logging_setup(root);
    write_ssh_accounts(root);
    write_devices(root);
    write_sync_items(root);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(pathname).parent_path(), ec);
    const wstring tmp_pathname = pathname + L".tmp";
    try {
        util::write_utf8_file(tmp_pathname, json::to_text(root));  // [Config.md#utf8-file]
    }
    catch (std::exception& ex) {
#ifdef _WXWIDGETS
        wxMessageBox(wxString(translation::translate("config_write_error")) + ex.what());
#endif
        log_fatal(translation::translate_utf8("config_write_error"), ex.what());
        util::remove_file(tmp_pathname);
        return false;
    }
    if (!util::rename_file(tmp_pathname, pathname))
        return false;
    disk_time_ = disk_time(pathname);
    changed_ = false;
    return true;
}

bool Config::file_changed_on_disk() const
{
    return disk_time(pathname_) != disk_time_;
}

bool Config::read(std::wstring_view pathname)
{
    reset();
    ready_ = false;
    disk_time_ = disk_time(wstring(pathname));  // [Config.md#disk_time_]
    if (!interpret_json(parse_json_file(pathname.data()))) {
        reset();
#ifdef _WXWIDGETS
        wxMessageBox(wxString(translation::translate("config_not_readable")));
#endif
        log_fatal(translation::translate_utf8("config_not_readable"));
        return false;
    }
    ready_ = true;
    changed_= false;
    return true;
}

void Config::add(const SyncItem& si, bool loading)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    syncitems_.push_back(si);
    if (!loading)
        changed_ = true;
}

void Config::add(const SshAccount& account, bool loading)
{
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    ssh_accounts_.push_back(account);
    if (!loading)
        changed_ = true;
}

std::optional<SshAccount> Config::find_ssh_account_by_name(const std::string& name) const
{
    {
        std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
        for (const auto& acc : ssh_accounts_)
            if (acc.name() == name)
                return acc;
    }
    return device_account(name);  // [Config.md#device-accounts]
}

std::optional<SshAccount> Config::ssh_account_at(std::size_t index) const
{
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    if (index >= ssh_accounts_.size())
        return std::nullopt;
    return ssh_accounts_[index];
}

SshAccounts Config::ssh_accounts_snapshot() const
{
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    return ssh_accounts_;
}

bool Config::remove_ssh_account(std::size_t index)
{
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    if (index >= ssh_accounts_.size())
        return false;
    ssh_accounts_.erase(ssh_accounts_.begin() + index);
    changed_ = true;
    return true;
}

bool Config::update_ssh_account(std::size_t index, const SshAccount& updated)
{
    std::lock_guard<std::mutex> lock(ssh_accounts_mutex_);
    if (index >= ssh_accounts_.size())
        return false;
    ssh_accounts_[index] = updated;
    changed_ = true;
    return true;
}

bool Config::read_devices(boost::property_tree::wptree& root)  // [Config.md#devices]
{
    const auto devices_node = root.get_child_optional(L"Devices");
    if (devices_node == boost::none)
        return true;
    bool all_ok = true;
    for (const auto& [key, node] : devices_node.value()) {
        try {  // [Config.md#per-account-error-handling]
            Device device;
            device.id = util::wstring_to_string(node.get<wstring>(L"id"));
            device.name = util::wstring_to_string(node.get<wstring>(L"name"));
            device.cert_fingerprint = util::wstring_to_string(node.get<wstring>(L"certFingerprint"));
            device.ssh_host_key = util::wstring_to_string(node.get<wstring>(L"sshHostKey", L""));
            if (!valid_device_id(device.id))
                throw std::runtime_error("invalid device id");
            add(device, true);
        }
        catch (std::exception& ex) {
            log_error(translation::translate_utf8("device_entry_skipped"), ex.what());
            all_ok = false;
        }
    }
    return all_ok;
}

void Config::write_devices(boost::property_tree::wptree& root)
{
    pt::wptree devices_node;
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (const auto& device : devices_) {
        pt::wptree node;
        node.put(wstring(L"id"), string_to_wstring(device.id));
        node.put(wstring(L"name"), string_to_wstring(device.name));
        node.put(wstring(L"certFingerprint"), string_to_wstring(device.cert_fingerprint));
        node.put(wstring(L"sshHostKey"), string_to_wstring(device.ssh_host_key));
        devices_node.push_back(make_pair(wstring(L"Device"), node));
    }
    root.push_back(make_pair(wstring(L"Devices"), devices_node));
}

void Config::add(const Device& device, bool loading)
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.push_back(device);
    if (!loading)
        changed_ = true;
}

std::optional<Device> Config::find_device(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    const auto found = std::ranges::find(devices_, id, &Device::id);
    return found == devices_.end() ? std::nullopt : std::optional<Device>(*found);
}

Devices Config::devices_snapshot() const
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_;
}

bool Config::update_device(const Device& updated)
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    const auto found = std::ranges::find(devices_, updated.id, &Device::id);
    if (found == devices_.end())
        return false;
    *found = updated;
    changed_ = true;
    return true;
}

bool Config::remove_device(const std::string& id)
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    const auto found = std::ranges::find(devices_, id, &Device::id);
    if (found == devices_.end())
        return false;
    devices_.erase(found);
    changed_ = true;
    return true;
}

void Config::set_device_account(const SshAccount& account)  // [Config.md#device-accounts]
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    device_accounts_[account.name()] = account;
}

void Config::clear_device_account(const std::string& id)
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    device_accounts_.erase(id);
}

std::optional<SshAccount> Config::device_account(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(devices_mutex_);
    const auto live = device_accounts_.find(id);
    if (live != device_accounts_.end())
        return live->second;
    const auto device = std::ranges::find(devices_, id, &Device::id);
    if (device == devices_.end())
        return std::nullopt;
    SshAccount placeholder;  // [Config.md#device-placeholder]
    placeholder.set_name(device->id);
    placeholder.set_host_key(device->ssh_host_key);
    return placeholder;
}

void Config::set(unsigned i, const SyncItem& si)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    syncitems_[i]=si;
    changed_ = true;
}

void Config::remove(unsigned i)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    auto iter=syncitems_.begin();
    std::advance(iter, i);
    remove_sync_state(sync_state_file_path(sync_state_dir(), iter->name()));  // [Config.md#state-leaves-with-item]
    syncitems_.erase(iter);
    changed_ = true;
}

bool Config::erase_by_name(const std::wstring& name)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (auto iter = syncitems_.begin(); iter != syncitems_.end(); ++iter) {
        if (iter->name() == name) {
            remove_sync_state(sync_state_file_path(sync_state_dir(), name));  // [Config.md#state-leaves-with-item]
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
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    return syncitems_[i];
}

SyncItems Config::syncitems() const
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    return syncitems_;
}

void Config::refresh_last_sync(const std::wstring& name)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (SyncItem& i : syncitems_) {
        if (i.name() == name) {
            i.set_last_sync();
            return;
        }
    }
}

void Config::record_last_run(const std::wstring& name, const LastRun& run)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (SyncItem& i : syncitems_) {
        if (i.name() == name) {
            i.last_run(run);
            return;
        }
    }
}

std::optional<SyncItem> Config::find(const std::wstring& path, bool source) const
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (const SyncItem& i: syncitems_) {
        if ((source && i.source() == path) || (!source && i.target() == path))
            return i;
    }
    return std::nullopt;
}

std::optional<SyncItem> Config::find_by_name(const std::wstring& name) const
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (const SyncItem& i : syncitems_) {
        if (i.name() == name)
            return i;
    }
    return std::nullopt;
}

bool Config::update_by_name(const std::wstring& name, const SyncItem& updated)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (SyncItem& i : syncitems_) {
        if (i.name() == name) {
            i = updated;
            changed_ = true;
            return true;
        }
    }
    return false;
}

bool Config::update_ssh_account_references(const std::string& old_name, const SshAccount& new_account)
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    bool any = false;
    for (SyncItem& i : syncitems_)
        if (i.rename_ssh_account_reference(old_name, new_account))
            any = true;
    if (any)
        changed_ = true;
    return any;
}

bool Config::ssh_account_referenced_by_any_item(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(syncitems_mutex_);
    for (const SyncItem& i : syncitems_)
        if (i.references_ssh_account(name))
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
