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
#include "UserActions.h"
#include "EventPump.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/toolbar.h>
#include <wx/uiaction.h>
#include <cctype>
#include <string_view>
#ifdef _WIN32
    #include <wx/msw/wrapwin.h>
#elif defined(_MACOS)
    #include <ApplicationServices/ApplicationServices.h>
    #include <unistd.h>
#else
    #include <X11/XKBlib.h>
#endif

namespace guitest {
namespace {

wxPoint centerOnScreen(wxWindow& control)
{
    const wxSize size = control.GetSize();
    return control.GetScreenPosition() + wxPoint(size.x / 2, size.y / 2);
}

#ifdef _WIN32  // [UserActions.md#why-not-simulator-text]
bool typeCharacter(wxUIActionSimulator&, wxUniChar character)  // [UserActions.md#windows-injects-the-character]
{
    const wxUint32 value = character.GetValue();
    if (value > 0xFFFF)
        return false; // outside the BMP a single wScan cannot carry it
    INPUT input[2] = {};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wScan = static_cast<WORD>(value);
    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
    input[1] = input[0];
    input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    return SendInput(2, input, sizeof(INPUT)) == 2;
}
#elif defined(_MACOS)
bool typeCharacter(wxUIActionSimulator&, wxUniChar character)  // [UserActions.md#macos-injects-the-character]
{
    const wxUint32 value = character.GetValue();
    if (value > 0xFFFF)
        return false; // outside the BMP a single UniChar cannot carry it
    UniChar unicode = static_cast<UniChar>(value);
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, 0, false);
    if (down == nullptr || up == nullptr) {
        if (down != nullptr)
            CFRelease(down);
        if (up != nullptr)
            CFRelease(up);
        return false;
    }
    CGEventKeyboardSetUnicodeString(down, 1, &unicode);
    CGEventKeyboardSetUnicodeString(up, 1, &unicode);
    CGEventPostToPid(getpid(), down);
    CGEventPostToPid(getpid(), up);
    CFRelease(down);
    CFRelease(up);
    return true;
}
#else
Display* keyboardDisplay()  // [UserActions.md#keyboardDisplay]
{
    static Display* display = XOpenDisplay(nullptr);
    return display;
}

bool typeCharacter(wxUIActionSimulator& simulator, wxUniChar character)  // [UserActions.md#x11-needs-the-layout]
{
    if (!character.IsAscii())
        return false;
    Display* display = keyboardDisplay();
    if (display == nullptr)
        return false;
    const KeySym keysym = character.GetValue();  // [UserActions.md#ascii-equals-keysym]
    const KeyCode key = XKeysymToKeycode(display, keysym);
    if (key == 0)
        return false;
    const bool needsShift = XkbKeycodeToKeysym(display, key, 0, 0) != keysym;
    return simulator.Char(character.GetValue(), needsShift ? wxMOD_SHIFT : wxMOD_NONE);
}
#endif

}

wxWindow* find(wxWindow& parent, wxWindowID id)
{
    return wxWindow::FindWindowById(id, &parent);
}

wxString valueOf(wxWindow* control)
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(control);
    return textCtrl != nullptr ? textCtrl->GetValue() : wxString();
}

bool clickOn(wxWindow* control)
{
    if (control == nullptr || !control->IsShownOnScreen())
        return false;
    wxUIActionSimulator simulator;
    if (!simulator.MouseMove(centerOnScreen(*control)))  // [UserActions.md#pointer-move-settles-focus]
        return false;
    pump(20);
    if (!simulator.MouseClick())
        return false;
    pump(20);
    return true;
}

bool clickOn(wxTreeCtrl& tree, wxTreeItemId item)
{
    if (!item.IsOk() || !tree.IsShownOnScreen())
        return false;
    wxRect rect;
    if (!tree.GetBoundingRect(item, rect, true))  // [UserActions.md#caller-ensures-visibility]
        return false;
    wxUIActionSimulator simulator;
    const wxPoint screenPoint = tree.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2));
    if (!simulator.MouseMove(screenPoint))
        return false;
    pump(20);
    if (!simulator.MouseClick())
        return false;
    pump(20);
    return true;
}

namespace {

bool clickAt(const wxPoint& screenPoint, bool doubleClick)
{
    wxUIActionSimulator simulator;
    if (!simulator.MouseMove(screenPoint))
        return false;
    pump(20);
    if (!(doubleClick ? simulator.MouseDblClick() : simulator.MouseClick()))
        return false;
    pump(20);
    return true;
}

}

bool clickOn(wxListCtrl& list, long item, bool doubleClick)  // [UserActions.md#clickOn-list-item]
{
    wxRect rect;
    if (!list.IsShownOnScreen() || !list.GetItemRect(item, rect))
        return false;
    return clickAt(list.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2)), doubleClick);
}

bool clickOnRowLabel(wxGrid& grid, int row)  // [UserActions.md#clickOnRowLabel]
{
    if (!grid.IsShownOnScreen() || row < 0 || row >= grid.GetNumberRows())
        return false;
    const wxRect cell = grid.CellToRect(row, 0);
    int x = 0, y = 0;
    grid.CalcScrolledPosition(0, cell.y, &x, &y);
    wxWindow* labels = grid.GetGridRowLabelWindow();
    return clickAt(labels->ClientToScreen(wxPoint(grid.GetRowLabelSize() / 2, y + cell.height / 2)), false);
}

bool changeGridCell(wxGrid& grid, int row, int col, const wxString& value)  // [UserActions.md#changeGridCell]
{
    if (row < 0 || row >= grid.GetNumberRows() || col < 0 || col >= grid.GetNumberCols())
        return false;
    grid.SetCellValue(row, col, value);
    wxGridEvent event(grid.GetId(), wxEVT_GRID_CELL_CHANGED, &grid, row, col);
    grid.GetEventHandler()->ProcessEvent(event);
    pump();
    return true;
}

namespace {

bool typeText(const wxString& text)
{
    wxUIActionSimulator simulator;
    for (const wxUniChar character : text) {
        if (!typeCharacter(simulator, character))
            return false;
    }
    pump();
    return true;
}

}

bool typeInto(wxWindow* field, const wxString& text)
{
    if (!clickOn(field) || !typeText(text))
        return false;
    return valueOf(field) == text;
}

bool replaceText(wxWindow* field, const wxString& text)
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(field);
    if (textCtrl == nullptr || !clickOn(field))
        return false;
    textCtrl->SelectAll();  // [UserActions.md#select-after-the-click]
    if (!typeText(text))
        return false;
    return valueOf(field) == text;
}

bool replaceLines(wxWindow* field, const wxString& lines)  // [UserActions.md#replaceLines]
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(field);
    if (textCtrl == nullptr || !textCtrl->IsMultiLine() || !clickOn(field))
        return false;
    textCtrl->SelectAll();
    wxUIActionSimulator simulator;
    bool first = true;
    for (const wxString& line : wxSplit(lines, '\n')) {
        if (!first && !simulator.Char(WXK_RETURN))
            return false;
        if (!typeText(line))
            return false;
        first = false;
    }
    return valueOf(field) == lines;
}

namespace {

void sendCommand(wxWindow& control, wxEventType type, int value)
{
    wxCommandEvent event(type, control.GetId());
    event.SetEventObject(&control);
    event.SetInt(value);
    control.GetEventHandler()->ProcessEvent(event);
    pump();
}

}

bool selectRadioOption(wxWindow* radioBox, int option)  // [UserActions.md#selectRadioOption]
{
    wxRadioBox* box = dynamic_cast<wxRadioBox*>(radioBox);
    if (box == nullptr || option < 0 || option >= static_cast<int>(box->GetCount()))
        return false;
    box->SetSelection(option);
    sendCommand(*box, wxEVT_RADIOBOX, option);
    return box->GetSelection() == option;
}

bool selectEntry(wxWindow* control, const wxString& entry)  // [UserActions.md#selectEntry]
{
    wxControlWithItems* box = dynamic_cast<wxControlWithItems*>(control);
    const int index = box != nullptr ? box->FindString(entry) : wxNOT_FOUND;
    if (index == wxNOT_FOUND)
        return false;
    box->SetSelection(index);
    sendCommand(*box, dynamic_cast<wxListBox*>(control) != nullptr ? wxEVT_LISTBOX : wxEVT_CHOICE, index);
    return box->GetSelection() == index;
}

bool setCheckBox(wxWindow* checkBox, bool checked)  // [UserActions.md#setCheckBox]
{
    wxCheckBox* box = dynamic_cast<wxCheckBox*>(checkBox);
    if (box == nullptr || !box->IsEnabled())
        return false;
    if (box->GetValue() != checked && !clickOn(box))
        return false;
    return box->GetValue() == checked;
}

bool setSpinValue(wxWindow* spin, int value)  // [UserActions.md#setSpinValue]
{
    wxSpinCtrl* control = dynamic_cast<wxSpinCtrl*>(spin);
    if (control == nullptr)
        return false;
    control->SetValue(value);
    sendCommand(*control, wxEVT_SPINCTRL, control->GetValue());
    return control->GetValue() == value;
}

bool showNotebookPage(wxWindow& dialog, const wxString& pageTitle)  // [UserActions.md#showNotebookPage]
{
    for (wxWindow* child : dialog.GetChildren()) {
        wxNotebook* notebook = dynamic_cast<wxNotebook*>(child);
        if (notebook == nullptr)
            continue;
        for (size_t page = 0; page < notebook->GetPageCount(); ++page) {
            if (notebook->GetPageText(page) != pageTitle)
                continue;
            notebook->ChangeSelection(page);
            pump();
            return notebook->GetSelection() == static_cast<int>(page);
        }
    }
    return false;
}

bool clickWhenEnabled(wxWindow* control, int timeoutMilliseconds)
{
    if (control == nullptr)
        return false;
    if (!waitUntil([control] { return control->IsEnabled(); }, timeoutMilliseconds))  // [UserActions.md#update-ui-needs-idle]
        return false;
    return clickOn(control);
}

void activateMenuItem(wxFrame& frame, int menuItemId)
{
    wxCommandEvent event(wxEVT_MENU, menuItemId);  // [UserActions.md#activateMenuItem]
    event.SetEventObject(&frame);
    frame.GetEventHandler()->ProcessEvent(event);
}

bool activateToolbarTool(wxFrame& frame, int toolId)  // [UserActions.md#activateToolbarTool]
{
    wxToolBar* toolbar = frame.GetToolBar();
    if (toolbar == nullptr || toolbar->FindById(toolId) == nullptr || !toolbar->GetToolEnabled(toolId))
        return false;
    wxCommandEvent event(wxEVT_TOOL, toolId);
    event.SetEventObject(toolbar);
    toolbar->GetEventHandler()->ProcessEvent(event);
    pump();
    return true;
}

namespace {

wxTreeItemId childNamed(wxTreeCtrl& tree, wxTreeItemId parent, const wxString& label)
{
    wxTreeItemIdValue cookie;
    for (wxTreeItemId child = tree.GetFirstChild(parent, cookie); child.IsOk();
         child = tree.GetNextChild(parent, cookie)) {
        if (tree.GetItemText(child) == label)
            return child;
    }
    return wxTreeItemId();
}

}

wxTreeItemId findTreeItemForPath(wxTreeCtrl& tree, const std::filesystem::path& path)
{
    wxTreeItemId current = tree.GetRootItem();
    for (const auto& segment : path) {
        if (segment == path.root_directory())
            continue;
        current = childNamed(tree, current, segment.wstring());
        if (!current.IsOk())
            return current;
    }
    return current;
}

}
