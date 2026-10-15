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
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace AFS_Core {

namespace help_knowledge {  // [HelpKnowledge.md#help_knowledge]
struct Section {
    std::wstring page;
    std::wstring heading;  // empty for the text before the first subheading
    std::wstring text;
};
std::vector<Section> sections_of(const std::wstring& html);  // [HelpKnowledge.md#sections_of]
std::wstring excerpt_for(const std::wstring& question, const std::vector<Section>& sections, std::size_t max_chars);  // [HelpKnowledge.md#excerpt_for]
}

} // namespace AFS_Core
