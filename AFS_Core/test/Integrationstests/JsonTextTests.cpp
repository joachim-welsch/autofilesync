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
#include "JsonText.h"
#include "Util.h"
#include <boost/property_tree/json_parser.hpp>
#include <sstream>
#include <stdexcept>

using namespace AFS_Core;
namespace pt = boost::property_tree;

namespace {

pt::wptree parsed(const std::wstring& text)
{
    pt::wptree root;
    std::wistringstream in(text);
    pt::read_json(in, root);
    return root;
}

}

TEST(JsonTextTests, KeepsEveryCharacterReadable)  // [JsonTextTests.md#KeepsEveryCharacterReadable]
{
    pt::wptree root;
    root.put(L"Стан", L"Успішно");
    root.put(L"Name", L"Größe \U0001F600");

    const std::wstring text = json::to_text(root);

    EXPECT_NE(text.find(L"\"Стан\": \"Успішно\""), std::wstring::npos) << util::wstring_to_string(text);
    EXPECT_NE(text.find(L"Größe \U0001F600"), std::wstring::npos) << util::wstring_to_string(text);
    EXPECT_EQ(text.find(L"\\u"), std::wstring::npos) << "Ein Zeichen wurde maskiert: " << util::wstring_to_string(text);
    EXPECT_EQ(parsed(text).get<std::wstring>(L"Name"), L"Größe \U0001F600");
}

TEST(JsonTextTests, EscapesOnlyWhatJsonRequires)
{
    pt::wptree root;
    root.put(L"Wert", L"\"a\\b\"\n\t\x01/c");

    const std::wstring text = json::to_text(root);

    EXPECT_NE(text.find(L"\"\\\"a\\\\b\\\"\\n\\t\\u0001/c\""), std::wstring::npos) << util::wstring_to_string(text);
    EXPECT_EQ(parsed(text).get<std::wstring>(L"Wert"), L"\"a\\b\"\n\t\x01/c");
}

TEST(JsonTextTests, LayoutMatchesWriteJson)  // [JsonTextTests.md#LayoutMatchesWriteJson]
{
    pt::wptree entry;
    entry.put(L"Name", L"sicherung");
    entry.put(L"Files", L"3");
    pt::wptree list;
    list.push_back(std::make_pair(std::wstring(), entry));
    list.push_back(std::make_pair(std::wstring(), entry));
    pt::wptree root;
    root.put(L"Settings.Language", L"de");
    root.add_child(L"Entries", list);
    root.add_child(L"Empty", pt::wptree());
    std::wostringstream boost_text;

    pt::write_json(boost_text, root);

    EXPECT_EQ(json::to_text(root), boost_text.str());
}

TEST(JsonTextTests, EmptyRootIsAnEmptyObject)
{
    EXPECT_EQ(parsed(json::to_text(pt::wptree())).size(), 0u);
}

TEST(JsonTextTests, ValueWithChildrenIsRejected)
{
    pt::wptree root;
    root.put(L"Knoten", L"Wert");
    root.put(L"Knoten.Kind", L"auch");

    EXPECT_THROW(json::to_text(root), std::invalid_argument);
}
