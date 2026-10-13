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
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include "StrUtil.h"
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

std::optional<SyncItem> item_from(const boost::property_tree::wptree& pt)
{
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

std::optional<Proposal> proposal_from(const wstring& json)
{
    boost::property_tree::wptree pt;
    try {
        std::wistringstream in(json);
        boost::property_tree::read_json(in, pt);
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
    if (const wstring name = pt.get(L"delete", L""); !name.empty())  // [SyncProposal.md#Deletion]
        return Deletion{name};
    if (const auto item = item_from(pt))
        return *item;
    return std::nullopt;
}

Block proposal_block(const wstring& answer)
{
    for (Block block = fenced_block(answer, 0); block.begin != wstring::npos; block = fenced_block(answer, block.end))
        if (proposal_from(block.body))
            return block;
    const Block bare = bare_json(answer);
    return proposal_from(bare.body) ? bare : Block{};
}

wstring trimmed(wstring text)
{
    const auto not_space = [](wchar_t c) { return c != L' ' && c != L'\n' && c != L'\r' && c != L'\t'; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

wstring lower(wstring text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return std::towlower(c); });
    return text;
}

std::string gbnf_literal(const wstring& text)  // [SyncProposal.md#gbnf_literal]
{
    std::string literal = "\"\\\"";
    for (const char c : util::wstring_to_string(text))
        literal += c == '"' ? "\\\\\\\"" : c == '\\' ? "\\\\\\\\" : std::string(1, c);  // JSON escape, then GBNF escape
    return literal + "\\\"\"";
}

std::string alternatives(const std::vector<wstring>& texts)
{
    std::string rule;
    for (const wstring& text : texts)
        rule += (rule.empty() ? "" : " | ") + gbnf_literal(text);
    return rule;
}
}

wstring instruction()
{
    wstring types;
    for (const SyncItemType type : {SyncItemType::File, SyncItemType::Dir, SyncItemType::Tree})
        types += (types.empty() ? L"" : L", ") + sync_item_type_to_str(type);
    return translation::translate("assistant_sync_proposal_instruction", {types, sync_item_type_to_str(SyncItemType::Tree)});
}

std::optional<Proposal> parse(const wstring& answer)
{
    const Block block = proposal_block(answer);
    return block.begin == wstring::npos ? std::nullopt : proposal_from(block.body);
}

wstring without_block(const wstring& answer)
{
    const Block block = proposal_block(answer);
    if (block.begin == wstring::npos)
        return answer;
    return trimmed(answer.substr(0, block.begin) + answer.substr(block.end));
}

wstring describe(const Proposal& proposal)
{
    if (const auto* deletion = std::get_if<Deletion>(&proposal))
        return translation::translate("assistant_sync_deletion_summary", {deletion->name, translation::translate("chat_delete_sync")});
    const SyncItem& item = std::get<SyncItem>(proposal);
    return translation::translate("assistant_sync_proposal_summary", {item.name(), sync_item_type_to_str(item.sync_item_type()),
        item.source(), item.target(), translation::translate("chat_create_sync")});
}

bool names_two_paths(const wstring& question)
{
    std::wistringstream words(question);
    int paths = 0;
    for (wstring word; words >> word;)
        if (word.find_first_of(L"/\\") != wstring::npos && ++paths == 2)  // [SyncProposal.md#names_two_paths]
            return true;
    return false;
}

bool names_configured_item(const wstring& question, const SyncItems& items)
{
    const wstring text = lower(question);
    return std::any_of(items.begin(), items.end(), [&text](const SyncItem& item) { return text.find(lower(item.name())) != wstring::npos; });
}

wstring extraction_question()
{
    return translation::translate("assistant_sync_proposal_extraction");
}

std::string grammar(const SyncItems& items)
{
    std::vector<wstring> types, names;
    for (const SyncItemType type : {SyncItemType::File, SyncItemType::Dir, SyncItemType::Tree})
        types.push_back(sync_item_type_to_str(type));
    for (const SyncItem& item : items)
        names.push_back(item.name());
    return std::string("root ::= object | ") + (names.empty() ? "" : "deletion | ") + "\"null\"\n"
        "object ::= \"{\" ws \"\\\"name\\\"\" ws \":\" ws string \",\" ws \"\\\"source\\\"\" ws \":\" ws string \",\" ws"
        " \"\\\"target\\\"\" ws \":\" ws string \",\" ws \"\\\"type\\\"\" ws \":\" ws type ws \"}\"\n"
        "type ::= " + alternatives(types) + "\n"
        + (names.empty() ? "" : "deletion ::= \"{\" ws \"\\\"delete\\\"\" ws \":\" ws configured ws \"}\"\nconfigured ::= " + alternatives(names) + "\n")  // [SyncProposal.md#configured-names]
        + "string ::= \"\\\"\" char* \"\\\"\"\n"
        "char ::= [^\"\\\\\\x00-\\x1F] | \"\\\\\" [\"\\\\/bfnrt]\n"
        "ws ::= [ \\t\\n]*\n";
}

wstring as_block(const wstring& json)
{
    return wstring(FENCE) + L"sync\n" + json + L"\n" + FENCE;
}

}

} // namespace AFS_Core
