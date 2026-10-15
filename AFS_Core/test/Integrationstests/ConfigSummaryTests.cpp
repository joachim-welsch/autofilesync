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
#include <string>
#include <vector>
#include "Translation.h"

using namespace AFS_Core;
using std::wstring;
using translation::translate;

namespace {

SyncItem item_named(const wstring& name)
{
    return SyncItem(name, SyncItemType::Tree, L"/quelle/" + name, L"/ziel/" + name, L"*");
}

SyncItems items_named(std::size_t count)
{
    SyncItems items;
    for (std::size_t i = 1; i <= count; ++i)
        items.push_back(item_named(L"sync" + std::to_wstring(i)));
    return items;
}

bool contains(const wstring& text, const wstring& part)
{
    return text.find(part) != wstring::npos;
}

}

TEST(ConfigSummaryTests, EmptyConfigurationCountsZero)
{
    const wstring summary = config_summary::describe({}, {});
    EXPECT_TRUE(contains(summary, translate("config_summary_count", {L"0"}))) << summary;
    EXPECT_FALSE(contains(summary, translate("config_summary_ssh_accounts"))) << summary;
}

TEST(ConfigSummaryTests, DescribesNamePathsStateAndOptions)
{
    SyncItems items = items_named(2);
    items[1].enabled(false);
    items[1].two_way_sync(true);
    items[1].sync_cycle(300);
    const wstring summary = config_summary::describe(items, {});
    EXPECT_TRUE(contains(summary, translate("config_summary_count", {L"2"}))) << summary;
    EXPECT_TRUE(contains(summary, L"\"sync1\" (" + sync_item_type_to_str(SyncItemType::Tree) + L", " + translate("active") + L"): /quelle/sync1 -> /ziel/sync1")) << summary;
    EXPECT_TRUE(contains(summary, L"\"sync2\" (" + sync_item_type_to_str(SyncItemType::Tree) + L", " + translate("inactive") + L")")) << summary;
    EXPECT_TRUE(contains(summary, translate("config_summary_cycle", {L"300"}))) << summary;
    EXPECT_TRUE(contains(summary, translate("option_two_way_sync"))) << summary;
    EXPECT_FALSE(contains(summary, translate("option_father_son_principle"))) << summary;  // [ConfigSummaryTests.md#only-enabled-options]
}

TEST(ConfigSummaryTests, SameConfigurationRendersIdentically)  // [ConfigSummaryTests.md#SameConfigurationRendersIdentically]
{
    const SyncItems items = items_named(3);
    EXPECT_EQ(config_summary::describe(items, {}), config_summary::describe(items, {}));
}

TEST(ConfigSummaryTests, ListsAtMostMaxItemsAndCountsTheRest)
{
    const SyncItems items = items_named(config_summary::MAX_ITEMS + 5);
    const wstring summary = config_summary::describe(items, {});
    EXPECT_TRUE(contains(summary, L"\"sync" + std::to_wstring(config_summary::MAX_ITEMS) + L"\"")) << summary;
    EXPECT_FALSE(contains(summary, L"\"sync" + std::to_wstring(config_summary::MAX_ITEMS + 1) + L"\"")) << summary;
    EXPECT_TRUE(contains(summary, translate("config_summary_more", {L"5"}))) << summary;
    EXPECT_TRUE(contains(summary, translate("config_summary_count", {std::to_wstring(config_summary::MAX_ITEMS + 5)}))) << summary;
}

TEST(ConfigSummaryTests, NamesSshAccountsWithoutPassword)
{
    SshAccount account;
    account.set_name("nas");
    account.set_user("pi");
    account.set_host("192.168.1.2");
    account.set_password("streng-geheim");
    const wstring summary = config_summary::describe({}, {account});
    EXPECT_TRUE(contains(summary, L"nas (pi@192.168.1.2)")) << summary;
    EXPECT_FALSE(contains(summary, L"streng-geheim")) << summary;
}

TEST(ConfigSummaryTests, ItemNotYetSynchronisedSaysSo)
{
    EXPECT_TRUE(contains(config_summary::describe({item_named(L"neu")}, {}), translate("config_summary_never_synced")));
}

TEST(ConfigSummaryTests, LastSyncIsGivenInMinutes)  // [ConfigSummaryTests.md#LastSyncIsGivenInMinutes]
{
    SyncItem item = item_named(L"eben");
    item.set_last_sync();
    const wstring summary = config_summary::describe({item}, {});
    EXPECT_TRUE(contains(summary, translate("config_summary_last_sync", {L"0"}))) << summary;
    EXPECT_FALSE(contains(summary, translate("config_summary_never_synced"))) << summary;
}

TEST(ConfigSummaryTests, UnreachableLastRunIsNamed)
{
    SyncItem item = item_named(L"weg");
    item.last_run({false, 1, L"nicht erreichbar"});
    const wstring summary = config_summary::describe({item}, {});
    EXPECT_TRUE(contains(summary, translate("config_summary_unreachable"))) << summary;
}

TEST(ConfigSummaryTests, ErrorsOfTheLastRunAreNamedWithTheFirst)  // [ConfigSummaryTests.md#ErrorsOfTheLastRunAreNamedWithTheFirst]
{
    SyncItem item = item_named(L"fehler");
    item.last_run({true, 3, L"Zugriff verweigert: /ziel/a.txt" + wstring(500, L'x')});
    const wstring summary = config_summary::describe({item}, {});
    EXPECT_TRUE(contains(summary, translate("config_summary_last_errors", {L"3", L"Zugriff verweigert: /ziel/a.txt"}).substr(0, 20))) << summary;
    EXPECT_FALSE(contains(summary, wstring(300, L'x'))) << "Der Fehlertext wurde nicht gekürzt.";
}
