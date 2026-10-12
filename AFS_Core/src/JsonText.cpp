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
#include <cstdint>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace AFS_Core {

namespace json {
namespace {
using boost::property_tree::wptree;

std::wstring control_escape(wchar_t c)
{
    const wchar_t* hex = L"0123456789ABCDEF";
    const auto code = static_cast<std::uint32_t>(c);
    return std::wstring(L"\\u00") + hex[code >> 4] + hex[code & 0xF];
}

std::wstring escaped(const std::wstring& text)  // [JsonText.md#escaped]
{
    std::wstring retval;
    retval.reserve(text.size());
    for (const wchar_t c : text) {
        switch (c) {
        case L'"': retval += L"\\\""; break;
        case L'\\': retval += L"\\\\"; break;
        case L'\b': retval += L"\\b"; break;
        case L'\f': retval += L"\\f"; break;
        case L'\n': retval += L"\\n"; break;
        case L'\r': retval += L"\\r"; break;
        case L'\t': retval += L"\\t"; break;
        default: retval += static_cast<std::uint32_t>(c) < 0x20 ? control_escape(c) : std::wstring(1, c);
        }
    }
    return retval;
}

bool representable(const wptree& node, bool root)  // [JsonText.md#representable]
{
    if (!node.data().empty() && (root || !node.empty()))
        return false;
    for (const auto& child : node)
        if (!representable(child.second, false))
            return false;
    return true;
}

void write(std::wostringstream& out, const wptree& node, int indent);

void write_children(std::wostringstream& out, const wptree& node, int indent, bool array)
{
    out << (array ? L'[' : L'{') << L'\n';
    for (auto child = node.begin(); child != node.end(); ++child) {
        out << std::wstring(4 * (indent + 1), L' ');
        if (!array)
            out << L'"' << escaped(child->first) << L"\": ";
        write(out, child->second, indent + 1);
        out << (std::next(child) != node.end() ? L",\n" : L"\n");
    }
    out << std::wstring(4 * indent, L' ') << (array ? L']' : L'}');
}

void write(std::wostringstream& out, const wptree& node, int indent)  // [JsonText.md#write]
{
    if (indent > 0 && node.empty())
        out << L'"' << escaped(node.data()) << L'"';
    else
        write_children(out, node, indent, indent > 0 && node.count(std::wstring()) == node.size());
}
}

std::wstring to_text(const wptree& root)
{
    if (!representable(root, true))
        throw std::invalid_argument("ptree contains data that cannot be represented in JSON format");
    std::wostringstream out;
    write(out, root, 0);
    out << L'\n';
    return out.str();
}
}

} // namespace AFS_Core
