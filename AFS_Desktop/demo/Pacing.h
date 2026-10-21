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
#pragma once

#include <wx/gdicmn.h>
#include <wx/string.h>
#include <optional>

class wxWindow;
class wxToolBar;
class wxNotebook;

namespace demo {  // [Pacing.md#Pacing]

constexpr int GLIDE_MILLISECONDS = 700;
constexpr int SHORT_DWELL = 800;
constexpr int DWELL = 1500;
constexpr int LONG_DWELL = 3000;

void set_speed(double factor);  // [Pacing.md#set_speed]
int paced(int milliseconds);
void dwell(int milliseconds);
wxPoint center_of(wxWindow& control);
bool glide_to(const wxPoint& screen_point, int milliseconds = GLIDE_MILLISECONDS);  // [Pacing.md#glide_to]
bool glide_to(wxWindow* control);
bool glide_click(wxWindow* control);  // [Pacing.md#glide_click]
bool press_key(int key_code, int modifiers = 0);  // [Pacing.md#press_key]
bool type_slowly(wxWindow* field, const wxString& text);  // [Pacing.md#type_slowly]
std::optional<wxRect> tool_rect(wxToolBar& toolbar, int tool_id);  // [Pacing.md#tool_rect]
std::optional<wxRect> tab_rect(wxNotebook& notebook, size_t page);  // [Pacing.md#tab_rect]
wxNotebook* notebook_of(wxWindow& dialog);

}
