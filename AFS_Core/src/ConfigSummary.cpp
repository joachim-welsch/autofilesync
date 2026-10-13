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
#include "ConfigSummary.h"
#include <algorithm>
#include <sstream>
#include "Config.h"
#include "Translation.h"
#include "Util.h"

namespace AFS_Core {

namespace config_summary {

namespace {
using std::wstring;
using translation::translate;

wstring joined(const std::vector<wstring>& parts, const wchar_t* separator)
{
    wstring result;
    for (const wstring& part : parts)
        result += (result.empty() ? L"" : separator) + part;
    return result;
}

wstring enabled_options(const SyncItem& item)
{
    const std::pair<bool, const char*> options[] = {
        {item.two_way_sync(), "option_two_way_sync"}, {item.father_son_principle(), "option_father_son_principle"},
        {item.delete_in_target(), "option_delete_in_target"}, {item.backup_proof(), "option_backup_proof"},
        {item.sync_empty_files(), "option_sync_empty_files"}, {item.del_aftercopy(), "option_delete_source_after_copy"},
        {item.copy_when_target_newer(), "option_copy_when_target_newer"}};
    std::vector<wstring> names;
    for (const auto& [enabled, msgid] : options)
        if (enabled)
            names.push_back(translate(msgid));
    return joined(names, L", ");
}

wstring compared_by(const SyncItem& item)
{
    const CompareItems compare = item.compare_items();
    const std::pair<bool, const char*> criteria[] = {
        {compare.time(), "compare_time"}, {compare.size(), "size"}, {compare.content(), "compare_content"}};
    std::vector<wstring> names;
    for (const auto& [chosen, msgid] : criteria)
        if (chosen)
            names.push_back(translate(msgid));
    return joined(names, L", ");
}

wstring describe_item(const SyncItem& item)  // [ConfigSummary.md#describe_item]
{
    std::vector<wstring> parts;
    parts.push_back(L"- \"" + item.name() + L"\" (" + item.item_type_str() + L", "
        + translate(item.enabled() ? "active" : "inactive") + L"): " + item.source() + L" -> " + item.target());
    parts.push_back(translate("config_summary_cycle", {std::to_wstring(item.sync_cycle())}));
    if (const wstring wildcard = item.wildcard(); !wildcard.empty() && wildcard != L"*")
        parts.push_back(translate("wildcard") + L": " + wildcard);
    if (const std::size_t exclusions = item.exclusions().size(); exclusions > 0)
        parts.push_back(translate("tab_exclusions") + L": " + std::to_wstring(exclusions));
    parts.push_back(translate("compare_by") + L" " + compared_by(item));
    if (const wstring options = enabled_options(item); !options.empty())
        parts.push_back(translate("tab_options") + L": " + options);
    return joined(parts, L"; ");
}

wstring describe_accounts(const std::vector<SshAccount>& accounts)  // [ConfigSummary.md#no-passwords]
{
    std::vector<wstring> names;
    for (const SshAccount& account : accounts)
        names.push_back(util::string_to_wstring(account.name() + " (" + account.user() + "@" + account.host() + ")"));
    return translate("config_summary_ssh_accounts") + joined(names, L", ");
}
}

wstring describe(const SyncItems& items, const std::vector<SshAccount>& accounts)
{
    std::vector<wstring> lines{translate("config_summary_intro"), translate("config_summary_count", {std::to_wstring(items.size())})};
    const std::size_t shown = std::min(items.size(), MAX_ITEMS);
    for (std::size_t i = 0; i < shown; ++i)
        lines.push_back(describe_item(items[i]));
    if (items.size() > shown)
        lines.push_back(translate("config_summary_more", {std::to_wstring(items.size() - shown)}));
    if (!accounts.empty())
        lines.push_back(describe_accounts(accounts));
    return joined(lines, L"\n");
}

wstring current()
{
    const Config& config = Config::instance();
    return describe(config.syncitems(), config.ssh_accounts_snapshot());
}

}

} // namespace AFS_Core
