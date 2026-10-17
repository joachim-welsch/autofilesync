/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "HelpBook.h"
#include <memory>
#include <string>
#include <wx/log.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include "Util.h"

using namespace AFS_Core;

namespace {
std::string content_of(wxZipInputStream& zip)
{
    std::string bytes;
    char buffer[4096];
    while (zip.Read(buffer, sizeof buffer).LastRead() > 0)
        bytes.append(buffer, zip.LastRead());
    return bytes;
}
}

std::vector<help_knowledge::Section> read_help_book(const wxString& path)
{
    std::vector<help_knowledge::Section> sections;
    const wxLogNull quiet;  // [HelpBook.md#quiet]
    wxFFileInputStream file(path);
    if (path.empty() || !file.IsOk())
        return sections;
    wxZipInputStream zip(file);
    for (std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry()); entry; entry.reset(zip.GetNextEntry())) {
        if (!entry->GetName().EndsWith(".html"))
            continue;
        const std::vector<help_knowledge::Section> page = help_knowledge::sections_of(util::string_to_wstring(content_of(zip)));
        sections.insert(sections.end(), page.begin(), page.end());
    }
    return sections;
}
