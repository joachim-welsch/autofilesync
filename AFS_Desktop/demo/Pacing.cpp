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
#include "Pacing.h"
#include "EventPump.h"
#include "UserActions.h"
#include <wx/notebook.h>
#include <wx/toolbar.h>
#include <wx/uiaction.h>
#include <wx/utils.h>
#include <wx/window.h>
#include <algorithm>
#include <cmath>

namespace demo {
namespace {

double speed_factor = 1.0;
constexpr int GLIDE_STEP_MILLISECONDS = 16;
constexpr int TYPING_DELAY_MILLISECONDS = 90;  // [Pacing.md#type_slowly]
constexpr int KEY_SETTLE_MILLISECONDS = 120;
constexpr int SCAN_STEP = 2;

double smoothstep(double t)  // [Pacing.md#glide_to]
{
    return t * t * (3 - 2 * t);
}

}

void set_speed(double factor)
{
    speed_factor = factor > 0 ? factor : 1.0;
}

int paced(int milliseconds)
{
    return static_cast<int>(std::lround(milliseconds / speed_factor));
}

void dwell(int milliseconds)
{
    guitest::pump(paced(milliseconds));
}

wxPoint center_of(wxWindow& control)
{
    const wxSize size = control.GetSize();
    return control.ClientToScreen(wxPoint(size.x / 2, size.y / 2));
}

bool glide_to(const wxPoint& screen_point, int milliseconds)
{
    wxUIActionSimulator simulator;
    const wxPoint from = wxGetMousePosition();
    const int steps = std::max(1, paced(milliseconds) / GLIDE_STEP_MILLISECONDS);
    for (int step = 1; step <= steps; ++step) {
        const double s = smoothstep(static_cast<double>(step) / steps);
        const wxPoint point(from.x + static_cast<int>(std::lround((screen_point.x - from.x) * s)),
                            from.y + static_cast<int>(std::lround((screen_point.y - from.y) * s)));
        if (!simulator.MouseMove(point))
            return false;
        guitest::pump(GLIDE_STEP_MILLISECONDS);
    }
    return simulator.MouseMove(screen_point);
}

bool glide_to(wxWindow* control)
{
    return control != nullptr && control->IsShownOnScreen() && glide_to(center_of(*control));
}

bool glide_click(wxWindow* control)  // [Pacing.md#glide_click]
{
    return glide_to(control) && guitest::click_on(control);
}

bool press_key(int key_code, int modifiers)
{
    wxUIActionSimulator simulator;
    if (!simulator.Char(key_code, modifiers))
        return false;
    guitest::pump(paced(KEY_SETTLE_MILLISECONDS));
    return true;
}

bool type_slowly(wxWindow* field, const wxString& text)  // [Pacing.md#type_slowly]
{
    if (!glide_to(field))
        return false;
    guitest::set_typing_delay(paced(TYPING_DELAY_MILLISECONDS));
    const bool typed = guitest::replace_text(field, text);
    guitest::set_typing_delay(0);
    return typed;
}

#ifdef __WXGTK__
extern "C" {  // [Pacing.md#tool_rect]
struct GtkAllocation { int x, y, width, height; };
void* gtk_toolbar_get_nth_item(void* toolbar, int n);
void gtk_widget_get_allocation(void* widget, GtkAllocation* allocation);
int gtk_widget_translate_coordinates(void* source, void* destination, int x, int y, int* dest_x, int* dest_y);
}

std::optional<wxRect> tool_rect(wxToolBar& toolbar, int tool_id)  // [Pacing.md#tool_rect]
{
    const int position = toolbar.GetToolPos(tool_id);
    void* item = position == wxNOT_FOUND ? nullptr : gtk_toolbar_get_nth_item(toolbar.GTKGetToolbar(), position);
    if (item == nullptr)
        return std::nullopt;
    GtkAllocation allocation{};
    gtk_widget_get_allocation(item, &allocation);
    int x = 0, y = 0;
    if (!gtk_widget_translate_coordinates(item, toolbar.GetHandle(), 0, 0, &x, &y))
        return std::nullopt;
    return wxRect(x, y, allocation.width, allocation.height);
}
#else
std::optional<wxRect> tool_rect(wxToolBar& toolbar, int tool_id)  // [Pacing.md#tool_rect]
{
    const wxSize size = toolbar.GetClientSize();
    int left = -1, right = -1;
    for (int x = 0; x < size.x; x += SCAN_STEP) {
        const wxToolBarToolBase* tool = toolbar.FindToolForPosition(x, size.y / 2);
        if (tool == nullptr || tool->GetId() != tool_id)
            continue;
        if (left < 0)
            left = x;
        right = x;
    }
    if (left < 0)
        return std::nullopt;
    return wxRect(left, 0, right - left + SCAN_STEP, size.y);
}
#endif

std::optional<wxRect> tab_rect(wxNotebook& notebook, size_t page)  // [Pacing.md#tab_rect]
{
    const wxSize size = notebook.GetClientSize();
    wxRect found;
    bool any = false;
    for (int y = 0; y < size.y / 4; y += SCAN_STEP)
        for (int x = 0; x < size.x; x += SCAN_STEP) {
            long flags = 0;
            if (notebook.HitTest(wxPoint(x, y), &flags) != static_cast<int>(page) || !(flags & wxBK_HITTEST_ONLABEL))
                continue;
            const wxRect point(x, y, SCAN_STEP, SCAN_STEP);
            found = any ? found.Union(point) : point;
            any = true;
        }
    return any ? std::optional<wxRect>(found) : std::nullopt;
}

wxNotebook* notebook_of(wxWindow& dialog)
{
    for (wxWindow* child : dialog.GetChildren())
        if (auto* notebook = dynamic_cast<wxNotebook*>(child))
            return notebook;
    return nullptr;
}

}
