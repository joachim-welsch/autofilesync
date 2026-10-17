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
#include "HelpKnowledge.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <set>

namespace AFS_Core {

namespace help_knowledge {
namespace {
using std::wstring;

constexpr double MIN_SCORE = 3.0;  // [HelpKnowledge.md#MIN_SCORE]
constexpr double TITLE_SCORE = 3.0;  // [HelpKnowledge.md#scored]
constexpr std::size_t MIN_WORD_LENGTH = 5;  // [HelpKnowledge.md#stems_of]
constexpr std::size_t MIN_ACRONYM_LENGTH = 3;
constexpr std::size_t MAX_ENDING_LENGTH = 2;

wstring between(const wstring& html, const wstring& open, const wstring& close, std::size_t from = 0)
{
    const std::size_t start = html.find(open, from);
    if (start == wstring::npos)
        return {};
    const std::size_t content = html.find(L'>', start);
    const std::size_t end = html.find(close, content);
    return content == wstring::npos || end == wstring::npos ? wstring() : html.substr(content + 1, end - content - 1);
}

wstring entity(const wstring& name)  // [HelpKnowledge.md#entity]
{
    static const std::pair<const wchar_t*, const wchar_t*> NAMED[] = {
        {L"rarr", L"→"}, {L"larr", L"←"}, {L"amp", L"&"}, {L"lt", L"<"}, {L"gt", L">"}, {L"quot", L"\""},
        {L"nbsp", L" "}, {L"auml", L"ä"}, {L"ouml", L"ö"}, {L"uuml", L"ü"}, {L"Auml", L"Ä"}, {L"Ouml", L"Ö"},
        {L"Uuml", L"Ü"}, {L"szlig", L"ß"}, {L"ndash", L"–"}, {L"hellip", L"…"}};
    for (const auto& [key, value] : NAMED)
        if (name == key)
            return value;
    if (name.size() > 1 && name[0] == L'#') {
        const bool hex = name[1] == L'x' || name[1] == L'X';
        return wstring(1, static_cast<wchar_t>(std::wcstoul(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10)));
    }
    return L"&" + name + L";";
}

wstring decoded(const wstring& text)
{
    wstring retval;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::size_t end = text[i] == L'&' ? text.find(L';', i) : wstring::npos;
        if (end == wstring::npos || end - i > 8) {
            retval += text[i];
            continue;
        }
        retval += entity(text.substr(i + 1, end - i - 1));
        i = end;
    }
    return retval;
}

wstring line_break_for(const wstring& tag)  // [HelpKnowledge.md#plain_text]
{
    if (tag == L"li")
        return L"\n- ";
    if (tag == L"br" || tag == L"/p" || tag == L"/tr" || tag == L"/li" || tag == L"h3" || tag == L"/h3" || tag == L"/table")
        return L"\n";
    return tag == L"/td" || tag == L"/th" ? L" " : L"";
}

wstring collapsed(const wstring& text)
{
    wstring retval;
    bool space = false;
    for (const wchar_t c : text) {
        if (c == L'\n') {
            while (!retval.empty() && retval.back() == L' ')
                retval.pop_back();
            if (!retval.empty() && retval.back() != L'\n')
                retval += L'\n';
            space = false;
        }
        else if (std::iswspace(c))
            space = true;
        else {
            if (space && !retval.empty() && retval.back() != L'\n')
                retval += L' ';
            retval += c;
            space = false;
        }
    }
    while (!retval.empty() && (retval.back() == L'\n' || retval.back() == L' '))
        retval.pop_back();
    return retval;
}

wstring plain_text(const wstring& html)
{
    wstring text;
    for (std::size_t i = 0; i < html.size(); ++i) {
        const std::size_t end = html[i] == L'<' ? html.find(L'>', i) : wstring::npos;
        if (end == wstring::npos) {
            text += html[i] == L'\n' ? L' ' : html[i];
            continue;
        }
        const wstring inner = html.substr(i + 1, end - i - 1);
        text += line_break_for(inner.substr(0, inner.find_first_of(L" \t\n")));
        i = end;
    }
    return collapsed(decoded(text));
}

wchar_t lowered(wchar_t c)
{
    return static_cast<wchar_t>(std::towlower(c));
}

bool word_char(wchar_t c)  // [HelpKnowledge.md#word_char]
{
    return static_cast<std::uint32_t>(c) > 127 || std::iswalnum(c);
}

wstring lowercase(wstring text)
{
    std::transform(text.begin(), text.end(), text.begin(), lowered);
    return text;
}

bool acronym(const wstring& word)
{
    return word.size() >= MIN_ACRONYM_LENGTH && std::ranges::all_of(word, [](wchar_t c) { return std::iswupper(c) != 0; });
}

std::set<wstring> stems_of(const wstring& question)  // [HelpKnowledge.md#stems_of]
{
    std::set<wstring> stems;
    wstring word;
    for (const wchar_t c : question + L' ') {
        if (word_char(c)) {
            word += c;
            continue;
        }
        if (acronym(word))
            stems.insert(lowercase(word));
        else if (word.size() >= MIN_WORD_LENGTH)
            stems.insert(lowercase(word.substr(0, std::max(MIN_WORD_LENGTH, word.size() - MAX_ENDING_LENGTH))));
        word.clear();
    }
    return stems;
}

struct Scored {
    double score = 0;
    const Section* section = nullptr;
};

std::vector<Scored> scored(const std::set<wstring>& stems, const std::vector<Section>& sections)  // [HelpKnowledge.md#scored]
{
    std::vector<wstring> bodies, titles;
    for (const Section& section : sections) {
        bodies.push_back(lowercase(section.text));
        titles.push_back(lowercase(section.page + L" " + section.heading));
    }
    std::vector<Scored> retval(sections.size());
    for (const wstring& stem : stems) {
        std::size_t containing = 0;
        for (const wstring& body : bodies)
            containing += body.find(stem) != wstring::npos;
        const double weight = containing == 0 ? 0 : std::log(static_cast<double>(sections.size() + 1) / static_cast<double>(containing));
        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (titles[i].find(stem) != wstring::npos)
                retval[i].score += TITLE_SCORE;
            if (bodies[i].find(stem) != wstring::npos)
                retval[i].score += weight;
        }
    }
    for (std::size_t i = 0; i < sections.size(); ++i)
        retval[i].section = &sections[i];
    std::ranges::stable_sort(retval, [](const Scored& a, const Scored& b) { return a.score > b.score; });
    return retval;
}

wstring labelled(const Section& section)
{
    return L"[" + section.page + (section.heading.empty() ? wstring() : L" – " + section.heading) + L"]\n" + section.text;
}

wstring cut_to(const wstring& text, std::size_t max_chars)
{
    if (text.size() <= max_chars)
        return text;
    const std::size_t line_end = text.rfind(L'\n', max_chars);
    return text.substr(0, line_end == wstring::npos ? max_chars : line_end);
}
}

std::vector<Section> sections_of(const std::wstring& html)
{
    const wstring page = plain_text(between(html, L"<h1", L"</h1>"));
    const std::size_t body = html.find(L"</h1>");
    std::size_t start = body == wstring::npos ? 0 : body + 5;
    wstring heading;
    std::vector<Section> sections;
    for (;;) {
        const std::size_t next = html.find(L"<h2", start);
        const wstring text = plain_text(html.substr(start, next == wstring::npos ? wstring::npos : next - start));
        if (!text.empty())
            sections.push_back({page, heading, text});
        if (next == wstring::npos)
            break;
        heading = plain_text(between(html, L"<h2", L"</h2>", next));
        const std::size_t close = html.find(L"</h2>", next);
        start = close == wstring::npos ? html.size() : close + 5;
    }
    return sections;
}

std::wstring excerpt_for(const std::wstring& question, const std::vector<Section>& sections, std::size_t max_chars)
{
    const std::vector<Scored> ranking = scored(stems_of(question), sections);
    if (ranking.empty() || ranking.front().score < MIN_SCORE)
        return {};
    wstring excerpt = cut_to(labelled(*ranking.front().section), max_chars);
    if (ranking.size() > 1 && ranking[1].score >= MIN_SCORE) {  // [HelpKnowledge.md#second-section]
        const wstring second = labelled(*ranking[1].section);
        if (excerpt.size() + 2 + second.size() <= max_chars)
            excerpt += L"\n\n" + second;
    }
    return excerpt;
}
}

} // namespace AFS_Core
