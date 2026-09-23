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
#include <functional>
#include <optional>
#include <string_view>
#include <vector>
#ifdef _WIN32
    #include <wx/msw/wrapwin.h>
#elif defined(_MACOS)
    #include <ApplicationServices/ApplicationServices.h>
    #include <unistd.h>
#else
    #include <X11/XKBlib.h>
    #include <X11/keysym.h>
    #include <X11/extensions/XTest.h>
#endif

namespace guitest {
namespace {

constexpr int SETTLE_MILLISECONDS = 30;  // [UserActions.md#wait-until-settled]
constexpr int SETTLE_TIMEOUT_MILLISECONDS = 1000;

bool settled(wxWindow& window)
{
    const wxPoint before = window.GetScreenPosition();
    pump(SETTLE_MILLISECONDS);
    return window.GetScreenPosition() == before;
}

void waitUntilSettled(wxWindow& window)  // [UserActions.md#wait-until-settled]
{
    waitUntil([&window] { return settled(window); }, SETTLE_TIMEOUT_MILLISECONDS);
}

constexpr int POINTER_TIMEOUT_MILLISECONDS = 500;

bool moveTo(wxUIActionSimulator& simulator, const wxPoint& screenPoint)  // [UserActions.md#pointer-arrives-before-click]
{
    if (!simulator.MouseMove(screenPoint))
        return false;
    waitUntil([screenPoint] { return wxGetMousePosition() == screenPoint; }, POINTER_TIMEOUT_MILLISECONDS);
    pump(20);
    return true;
}

wxPoint centerOnScreen(wxWindow& control)
{
    waitUntilSettled(control);
    const wxSize size = control.GetSize();
    return control.GetScreenPosition() + wxPoint(size.x / 2, size.y / 2);
}

#ifdef _WIN32  // [UserActions.md#why-not-simulator-text]
class Keyboard {  // [UserActions.md#windows-injects-the-character]
public:
    bool type(wxUniChar character) const
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
};
#elif defined(_MACOS)
class Keyboard {  // [UserActions.md#macos-injects-the-character]
public:
    bool type(wxUniChar character) const
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
};
#else
constexpr int EVENT_GAP_MILLISECONDS = 20;  // [UserActions.md#x11-event-spacing]

KeySym keysymOf(wxUniChar character)  // [UserActions.md#keysym-of-character]
{
    const wxUint32 value = character.GetValue();
    return value < 0x100 ? value : 0x01000000 | value;
}

KeySym deadKeyOf(KeySym keysym)  // [UserActions.md#x11-dead-keys]
{
    switch (keysym) {
    case XK_asciicircum: return XK_dead_circumflex;
    case XK_grave: return XK_dead_grave;
    case XK_asciitilde: return XK_dead_tilde;
    case XK_apostrophe: return XK_dead_acute;
    case XK_quotedbl: return XK_dead_diaeresis;
    default: return NoSymbol;
    }
}

class Keyboard {  // [UserActions.md#x11-keymap-lookup]
public:
    Keyboard();
    ~Keyboard();
    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;
    bool type(wxUniChar character) const;
private:
    struct Key { KeyCode code = 0; unsigned modifiers = 0; };
    Key keyOf(KeySym keysym) const;
    std::optional<unsigned> modifiersOf(int code, int group, int level) const;  // [UserActions.md#x11-level-modifiers]
    std::optional<std::vector<KeyCode>> modifierKeysOf(unsigned modifiers) const;
    KeyCode modifierKeyOf(unsigned bit) const;
    bool press(Key key) const;
    void fake(KeyCode code, bool down) const;
    Display* display_;
    XkbDescPtr keymap_ = nullptr;
    int group_ = 0;
};

Keyboard::Keyboard(): display_(XOpenDisplay(nullptr))
{
    if (display_ == nullptr)
        return;
    keymap_ = XkbGetMap(display_, XkbAllClientInfoMask, XkbUseCoreKbd);
    XkbStateRec state{};
    if (XkbGetState(display_, XkbUseCoreKbd, &state) == Success)
        group_ = state.group;
}

Keyboard::~Keyboard()
{
    if (keymap_ != nullptr)
        XkbFreeKeyboard(keymap_, XkbAllComponentsMask, True);
    if (display_ != nullptr)
        XCloseDisplay(display_);
}

bool Keyboard::type(wxUniChar character) const
{
    if (keymap_ == nullptr)
        return false;
    const KeySym keysym = keysymOf(character);
    if (const Key key = keyOf(keysym); key.code != 0)
        return press(key);
    const Key deadKey = keyOf(deadKeyOf(keysym));  // [UserActions.md#x11-dead-keys]
    return deadKey.code != 0 && press(deadKey) && press(keyOf(XK_space));
}

Keyboard::Key Keyboard::keyOf(KeySym keysym) const
{
    if (keysym == NoSymbol)
        return {};
    for (int code = keymap_->min_key_code; code <= keymap_->max_key_code; ++code) {
        const int groups = XkbKeyNumGroups(keymap_, code);
        if (groups == 0)
            continue;
        const int group = group_ < groups ? group_ : 0;
        for (int level = 0; level < XkbKeyGroupWidth(keymap_, code, group); ++level)
            if (XkbKeySymEntry(keymap_, code, level, group) == keysym)
                if (const std::optional<unsigned> modifiers = modifiersOf(code, group, level))
                    return {static_cast<KeyCode>(code), *modifiers};
    }
    return {};
}

std::optional<unsigned> Keyboard::modifiersOf(int code, int group, int level) const  // [UserActions.md#x11-level-modifiers]
{
    if (level == 0)
        return 0u;
    const XkbKeyTypePtr type = XkbKeyKeyType(keymap_, code, group);
    for (int entry = 0; entry < type->map_count; ++entry)
        if (type->map[entry].active && type->map[entry].level == level)
            return type->map[entry].mods.mask;
    return std::nullopt;
}

std::optional<std::vector<KeyCode>> Keyboard::modifierKeysOf(unsigned modifiers) const
{
    std::vector<KeyCode> keys;
    for (unsigned bit = 1; bit < 0x100; bit <<= 1) {
        if (!(modifiers & bit))
            continue;
        const KeyCode key = modifierKeyOf(bit);
        if (key == 0)
            return std::nullopt;
        keys.push_back(key);
    }
    return keys;
}

KeyCode Keyboard::modifierKeyOf(unsigned bit) const
{
    for (int code = keymap_->min_key_code; code <= keymap_->max_key_code; ++code)
        if (keymap_->map->modmap[code] & bit)
            return static_cast<KeyCode>(code);
    return 0;
}

bool Keyboard::press(Key key) const
{
    const std::optional<std::vector<KeyCode>> modifierKeys = key.code != 0 ? modifierKeysOf(key.modifiers) : std::nullopt;
    if (!modifierKeys)
        return false;
    for (KeyCode modifier : *modifierKeys)
        fake(modifier, true);
    fake(key.code, true);
    fake(key.code, false);
    for (auto modifier = modifierKeys->rbegin(); modifier != modifierKeys->rend(); ++modifier)
        fake(*modifier, false);
    return true;
}

void Keyboard::fake(KeyCode code, bool down) const
{
    XTestFakeKeyEvent(display_, code, down, CurrentTime);
    XSync(display_, False);
    wxMilliSleep(EVENT_GAP_MILLISECONDS);  // [UserActions.md#x11-event-spacing]
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
    if (!moveTo(simulator, centerOnScreen(*control)))  // [UserActions.md#pointer-move-settles-focus]
        return false;
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
    waitUntilSettled(tree);
    wxUIActionSimulator simulator;
    const wxPoint screenPoint = tree.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2));
    if (!moveTo(simulator, screenPoint))
        return false;
    if (!simulator.MouseClick())
        return false;
    pump(20);
    return true;
}

namespace {

bool clickAt(const wxPoint& screenPoint, bool doubleClick)
{
    wxUIActionSimulator simulator;
    if (!moveTo(simulator, screenPoint))
        return false;
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
    waitUntilSettled(list);
    return clickAt(list.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2)), doubleClick);
}

bool clickOnRowLabel(wxGrid& grid, int row)  // [UserActions.md#clickOnRowLabel]
{
    if (!grid.IsShownOnScreen() || row < 0 || row >= grid.GetNumberRows())
        return false;
    waitUntilSettled(grid);
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
    Keyboard keyboard;  // [UserActions.md#keyboard-per-text]
    for (const wxUniChar character : text)
        if (!keyboard.type(character))
            return false;
    pump();
    return true;
}

bool typeLines(const wxString& lines)  // [UserActions.md#replaceLines]
{
    wxUIActionSimulator simulator;
    bool first = true;
    for (const wxString& line : wxSplit(lines, '\n')) {
        if (!first && !simulator.Char(WXK_RETURN))
            return false;
        if (!typeText(line))
            return false;
        first = false;
    }
    return true;
}

constexpr int CLICK_ATTEMPTS = 3;
constexpr int FOCUS_TIMEOUT_MILLISECONDS = 200;

bool clickIntoField(wxTextCtrl& field)  // [UserActions.md#click-until-focused]
{
    for (int attempt = 0; attempt < CLICK_ATTEMPTS; ++attempt) {
        if (!clickOn(&field))
            return false;
        if (waitUntil([&field] { return field.HasFocus(); }, FOCUS_TIMEOUT_MILLISECONDS))
            return true;
        wxGetTopLevelParent(&field)->Raise();  // [UserActions.md#raise-when-not-focused]
        pump();
    }
    return true;
}

bool typedAndShown(wxTextCtrl& field, const wxString& wanted, const std::function<bool()>& type)  // [UserActions.md#retype-once]
{
    if (!type())
        return false;
    if (field.GetValue() == wanted)
        return true;
    field.SelectAll();
    return type() && field.GetValue() == wanted;
}

}

bool typeInto(wxWindow* field, const wxString& text)
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(field);
    if (textCtrl == nullptr || !clickIntoField(*textCtrl))
        return false;
    return typedAndShown(*textCtrl, text, [&text] { return typeText(text); });
}

bool replaceText(wxWindow* field, const wxString& text)
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(field);
    if (textCtrl == nullptr || !clickIntoField(*textCtrl))
        return false;
    textCtrl->SelectAll();  // [UserActions.md#select-after-the-click]
    return typedAndShown(*textCtrl, text, [&text] { return typeText(text); });
}

bool replaceLines(wxWindow* field, const wxString& lines)  // [UserActions.md#replaceLines]
{
    wxTextCtrl* textCtrl = dynamic_cast<wxTextCtrl*>(field);
    if (textCtrl == nullptr || !textCtrl->IsMultiLine() || !clickIntoField(*textCtrl))
        return false;
    textCtrl->SelectAll();
    return typedAndShown(*textCtrl, lines, [&lines] { return typeLines(lines); });
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
