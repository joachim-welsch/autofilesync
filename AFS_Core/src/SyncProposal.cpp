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
#include "SyncProposal.h"
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include "Translation.h"

namespace AFS_Core {

namespace sync_proposal {

namespace {
using std::wstring;
constexpr wchar_t FENCE[] = L"```";
constexpr wchar_t DEFAULT_WILDCARD[] = L"*.*";  // the SyncItemDialog default for a new item

struct Block { std::size_t begin = wstring::npos; std::size_t end = wstring::npos; wstring body; };

Block fenced_block(const wstring& text, std::size_t from)  // [SyncProposal.md#fenced-or-bare]
{
    Block block;
    block.begin = text.find(FENCE, from);
    if (block.begin == wstring::npos)
        return block;
    const std::size_t body_start = text.find(L'\n', block.begin);
    const std::size_t close = body_start == wstring::npos ? wstring::npos : text.find(FENCE, body_start);
    if (close == wstring::npos)
        return Block{};
    block.body = text.substr(body_start, close - body_start);
    block.end = close + wstring(FENCE).size();
    return block;
}

Block bare_json(const wstring& text)
{
    Block block;
    block.begin = text.find(L'{');
    const std::size_t close = text.rfind(L'}');
    if (block.begin == wstring::npos || close == wstring::npos || close < block.begin)
        return Block{};
    block.end = close + 1;
    block.body = text.substr(block.begin, block.end - block.begin);
    return block;
}

std::optional<SyncItem> item_from(const wstring& json)
{
    boost::property_tree::wptree pt;
    try {
        std::wistringstream in(json);
        boost::property_tree::read_json(in, pt);
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
    const wstring name = pt.get(L"name", L""), source = pt.get(L"source", L""), target = pt.get(L"target", L"");
    if (name.empty() || source.empty() || target.empty())  // [SyncProposal.md#required-fields]
        return std::nullopt;
    SyncItemType type = str_to_sync_item_type(pt.get(L"type", L""));
    if (type == SyncItemType::Undefined)
        type = SyncItemType::Tree;
    SyncItem item(name, type, source, target, pt.get(L"wildcard", wstring(DEFAULT_WILDCARD)));
    item.two_way_sync(pt.get(L"two_way", false));
    return item;
}

Block proposal_block(const wstring& answer)
{
    for (Block block = fenced_block(answer, 0); block.begin != wstring::npos; block = fenced_block(answer, block.end))
        if (item_from(block.body))
            return block;
    const Block bare = bare_json(answer);
    return item_from(bare.body) ? bare : Block{};
}

wstring trimmed(wstring text)
{
    const auto not_space = [](wchar_t c) { return c != L' ' && c != L'\n' && c != L'\r' && c != L'\t'; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}
}

wstring instruction()
{
    wstring types;
    for (const SyncItemType type : {SyncItemType::File, SyncItemType::Dir, SyncItemType::Tree})
        types += (types.empty() ? L"" : L", ") + sync_item_type_to_str(type);
    return translation::translate("assistant_sync_proposal_instruction", {types, sync_item_type_to_str(SyncItemType::Tree)});
}

std::optional<SyncItem> parse(const wstring& answer)
{
    const Block block = proposal_block(answer);
    return block.begin == wstring::npos ? std::nullopt : item_from(block.body);
}

wstring without_block(const wstring& answer)
{
    const Block block = proposal_block(answer);
    if (block.begin == wstring::npos)
        return answer;
    return trimmed(answer.substr(0, block.begin) + answer.substr(block.end));
}

wstring describe(const SyncItem& proposal)
{
    return translation::translate("assistant_sync_proposal_summary", {proposal.name(), sync_item_type_to_str(proposal.sync_item_type()),
        proposal.source(), proposal.target(), translation::translate("chat_create_sync")});
}

}

} // namespace AFS_Core
