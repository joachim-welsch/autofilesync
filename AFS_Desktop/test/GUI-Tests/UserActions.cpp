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
#include "ForegroundApp.h"
#include "ModalDialogWatcher.h"
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

void wait_until_settled(wxWindow& window)  // [UserActions.md#wait-until-settled]
{
    wait_until([&window] { return settled(window); }, SETTLE_TIMEOUT_MILLISECONDS);
}

constexpr int POINTER_TIMEOUT_MILLISECONDS = 500;
constexpr int CLICK_TIMEOUT_MILLISECONDS = 1000;  // [UserActions.md#click-arrives-later]
constexpr int SETTLE_AFTER_DIALOG_MILLISECONDS = 100;  // [UserActions.md#settle-after-dialog]

bool move_to(wxUIActionSimulator& simulator, const wxPoint& screen_point)  // [UserActions.md#pointer-arrives-before-click]
{
    if (!simulator.MouseMove(screen_point))
        return false;
    wait_until([screen_point] { return wxGetMousePosition() == screen_point; }, POINTER_TIMEOUT_MILLISECONDS);
    pump(20);
    return true;
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
constexpr CGKeyCode RETURN_KEY = 36;  // kVK_Return

CGEventFlags flags_of(int modifiers)  // [UserActions.md#macos-modifier-flags]
{
    CGEventFlags flags = 0;
    if (modifiers & wxMOD_SHIFT)
        flags |= kCGEventFlagMaskShift;
    if (modifiers & wxMOD_ALT)
        flags |= kCGEventFlagMaskAlternate;
    if (modifiers & wxMOD_CONTROL)
        flags |= kCGEventFlagMaskCommand;
    if (modifiers & wxMOD_RAW_CONTROL)
        flags |= kCGEventFlagMaskControl;
    return flags;
}

bool post_key_to_process(CGKeyCode key, CGEventFlags flags, const UniChar* unicode)  // [UserActions.md#macos-injects-the-character]
{
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, key, true);
    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, key, false);
    const bool created = down != nullptr && up != nullptr;
    for (CGEventRef event : {down, up}) {
        if (event == nullptr)
            continue;
        if (created) {
            CGEventSetFlags(event, flags);
            if (unicode != nullptr)
                CGEventKeyboardSetUnicodeString(event, 1, unicode);
            CGEventPostToPid(getpid(), event);
        }
        CFRelease(event);
    }
    return created;
}

class Keyboard {  // [UserActions.md#macos-injects-the-character]
public:
    bool type(wxUniChar character) const
    {
        const wxUint32 value = character.GetValue();
        if (value > 0xFFFF)
            return false; // outside the BMP a single UniChar cannot carry it
        const UniChar unicode = static_cast<UniChar>(value);
        return post_key_to_process(0, 0, &unicode);
    }
};
#else
constexpr int EVENT_GAP_MILLISECONDS = 20;  // [UserActions.md#x11-event-spacing]

KeySym keysym_of(wxUniChar character)  // [UserActions.md#keysym-of-character]
{
    const wxUint32 value = character.GetValue();
    return value < 0x100 ? value : 0x01000000 | value;
}

KeySym dead_key_of(KeySym keysym)  // [UserActions.md#x11-dead-keys]
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
    Key key_of(KeySym keysym) const;
    std::optional<unsigned> modifiers_of(int code, int group, int level) const;  // [UserActions.md#x11-level-modifiers]
    std::optional<std::vector<KeyCode>> modifier_keys_of(unsigned modifiers) const;
    KeyCode modifier_key_of(unsigned bit) const;
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
    const KeySym keysym = keysym_of(character);
    if (const Key key = key_of(keysym); key.code != 0)
        return press(key);
    const Key dead_key = key_of(dead_key_of(keysym));  // [UserActions.md#x11-dead-keys]
    return dead_key.code != 0 && press(dead_key) && press(key_of(XK_space));
}

Keyboard::Key Keyboard::key_of(KeySym keysym) const
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
                if (const std::optional<unsigned> modifiers = modifiers_of(code, group, level))
                    return {static_cast<KeyCode>(code), *modifiers};
    }
    return {};
}

std::optional<unsigned> Keyboard::modifiers_of(int code, int group, int level) const  // [UserActions.md#x11-level-modifiers]
{
    if (level == 0)
        return 0u;
    const XkbKeyTypePtr type = XkbKeyKeyType(keymap_, code, group);
    for (int entry = 0; entry < type->map_count; ++entry)
        if (type->map[entry].active && type->map[entry].level == level)
            return type->map[entry].mods.mask;
    return std::nullopt;
}

std::optional<std::vector<KeyCode>> Keyboard::modifier_keys_of(unsigned modifiers) const
{
    std::vector<KeyCode> keys;
    for (unsigned bit = 1; bit < 0x100; bit <<= 1) {
        if (!(modifiers & bit))
            continue;
        const KeyCode key = modifier_key_of(bit);
        if (key == 0)
            return std::nullopt;
        keys.push_back(key);
    }
    return keys;
}

KeyCode Keyboard::modifier_key_of(unsigned bit) const
{
    for (int code = keymap_->min_key_code; code <= keymap_->max_key_code; ++code)
        if (keymap_->map->modmap[code] & bit)
            return static_cast<KeyCode>(code);
    return 0;
}

bool Keyboard::press(Key key) const
{
    const std::optional<std::vector<KeyCode>> modifier_keys = key.code != 0 ? modifier_keys_of(key.modifiers) : std::nullopt;
    if (!modifier_keys)
        return false;
    for (KeyCode modifier : *modifier_keys)
        fake(modifier, true);
    fake(key.code, true);
    fake(key.code, false);
    for (auto modifier = modifier_keys->rbegin(); modifier != modifier_keys->rend(); ++modifier)
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

wxString value_of(wxWindow* control)
{
    wxTextCtrl* text_ctrl = dynamic_cast<wxTextCtrl*>(control);
    return text_ctrl != nullptr ? text_ctrl->GetValue() : wxString();
}

bool click_on(wxWindow& control, const wxPoint& client_point)
{
    if (!control.IsShownOnScreen())
        return false;
    wait_until_settled(control);
    wxUIActionSimulator simulator;
    if (!move_to(simulator, control.ClientToScreen(client_point)))  // [UserActions.md#pointer-move-settles-focus]
        return false;
    const int dialogs_before = ModalDialogWatcher::instance().dialogs_entered();
    if (!simulator.MouseClick())
        return false;
    pump(20);
    if (ModalDialogWatcher::instance().dialogs_entered() != dialogs_before)
        pump(SETTLE_AFTER_DIALOG_MILLISECONDS);  // [UserActions.md#settle-after-dialog]
    return true;
}

bool click_on(wxWindow* control)
{
    if (control == nullptr)
        return false;
    const wxSize size = control->GetSize();
    return click_on(*control, wxPoint(size.x / 2, size.y / 2));
}

bool click_on(wxTreeCtrl& tree, wxTreeItemId item)
{
    if (!item.IsOk() || !tree.IsShownOnScreen())
        return false;
    wxRect rect;
    if (!tree.GetBoundingRect(item, rect, true))  // [UserActions.md#caller-ensures-visibility]
        return false;
    wait_until_settled(tree);
    wxUIActionSimulator simulator;
    const wxPoint screen_point = tree.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2));
    if (!move_to(simulator, screen_point))
        return false;
    if (!simulator.MouseClick())
        return false;
    pump(20);
    return true;
}

namespace {

bool click_at(const wxPoint& screen_point, bool double_click)
{
    wxUIActionSimulator simulator;
    if (!move_to(simulator, screen_point))
        return false;
    if (!(double_click ? simulator.MouseDblClick() : simulator.MouseClick()))
        return false;
    pump(20);
    return true;
}

}

bool click_on(wxListCtrl& list, long item, bool double_click)  // [UserActions.md#click_on-list-item]
{
    wxRect rect;
    if (!list.IsShownOnScreen() || !list.GetItemRect(item, rect))
        return false;
    wait_until_settled(list);
    return click_at(list.ClientToScreen(rect.GetPosition() + wxPoint(rect.width / 2, rect.height / 2)), double_click);
}

bool click_on_row_label(wxGrid& grid, int row)  // [UserActions.md#click_on_row_label]
{
    if (!grid.IsShownOnScreen() || row < 0 || row >= grid.GetNumberRows())
        return false;
    wait_until_settled(grid);
    const wxRect cell = grid.CellToRect(row, 0);
    int x = 0, y = 0;
    grid.CalcScrolledPosition(0, cell.y, &x, &y);
    wxWindow* labels = grid.GetGridRowLabelWindow();
    return click_at(labels->ClientToScreen(wxPoint(grid.GetRowLabelSize() / 2, y + cell.height / 2)), false);
}

bool change_grid_cell(wxGrid& grid, int row, int col, const wxString& value)  // [UserActions.md#change_grid_cell]
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

bool type_text(const wxString& text)
{
    Keyboard keyboard;  // [UserActions.md#keyboard-per-text]
    for (const wxUniChar character : text)
        if (!keyboard.type(character))
            return false;
    pump();
    return true;
}

bool type_lines(const wxString& lines)  // [UserActions.md#replace_lines]
{
    bool first = true;
    for (const wxString& line : wxSplit(lines, '\n')) {
        if (!first && !press_return())
            return false;
        if (!type_text(line))
            return false;
        first = false;
    }
    return true;
}

constexpr int CLICK_ATTEMPTS = 3;
constexpr int FOCUS_TIMEOUT_MILLISECONDS = 200;

bool click_into_field(wxTextCtrl& field)  // [UserActions.md#click-until-focused]
{
    for (int attempt = 0; attempt < CLICK_ATTEMPTS; ++attempt) {
        if (!click_on(&field))
            return false;
        if (wait_until([&field] { return field.HasFocus(); }, FOCUS_TIMEOUT_MILLISECONDS))
            return true;
#ifdef _MACOS
        bring_to_foreground();  // [ForegroundApp.md#bring_to_foreground]
#endif
        wxGetTopLevelParent(&field)->Raise();  // [UserActions.md#raise-when-not-focused]
        pump();
    }
    return true;
}

bool typed_and_shown(wxTextCtrl& field, const wxString& wanted, const std::function<bool()>& type)  // [UserActions.md#retype-once]
{
    if (!type())
        return false;
    if (field.GetValue() == wanted)
        return true;
    field.SelectAll();
    return type() && field.GetValue() == wanted;
}

}

bool press_return(int modifiers)  // [UserActions.md#press_return]
{
#ifdef _MACOS
    return post_key_to_process(RETURN_KEY, flags_of(modifiers), nullptr);
#else
    return wxUIActionSimulator().Char(WXK_RETURN, modifiers);
#endif
}

bool type_into(wxWindow* field, const wxString& text)
{
    wxTextCtrl* text_ctrl = dynamic_cast<wxTextCtrl*>(field);
    if (text_ctrl == nullptr || !click_into_field(*text_ctrl))
        return false;
    return typed_and_shown(*text_ctrl, text, [&text] { return type_text(text); });
}

bool replace_text(wxWindow* field, const wxString& text)
{
    wxTextCtrl* text_ctrl = dynamic_cast<wxTextCtrl*>(field);
    if (text_ctrl == nullptr || !click_into_field(*text_ctrl))
        return false;
    text_ctrl->SelectAll();  // [UserActions.md#select-after-the-click]
    return typed_and_shown(*text_ctrl, text, [&text] { return type_text(text); });
}

bool replace_lines(wxWindow* field, const wxString& lines)  // [UserActions.md#replace_lines]
{
    wxTextCtrl* text_ctrl = dynamic_cast<wxTextCtrl*>(field);
    if (text_ctrl == nullptr || !text_ctrl->IsMultiLine() || !click_into_field(*text_ctrl))
        return false;
    text_ctrl->SelectAll();
    return typed_and_shown(*text_ctrl, lines, [&lines] { return type_lines(lines); });
}

namespace {

void send_command(wxWindow& control, wxEventType type, int value)
{
    wxCommandEvent event(type, control.GetId());
    event.SetEventObject(&control);
    event.SetInt(value);
    control.GetEventHandler()->ProcessEvent(event);
    pump();
}

}

bool select_radio_option(wxWindow* radio_box, int option)  // [UserActions.md#select_radio_option]
{
    wxRadioBox* box = dynamic_cast<wxRadioBox*>(radio_box);
    if (box == nullptr || option < 0 || option >= static_cast<int>(box->GetCount()))
        return false;
    box->SetSelection(option);
    send_command(*box, wxEVT_RADIOBOX, option);
    return box->GetSelection() == option;
}

bool select_entry(wxWindow* control, const wxString& entry)  // [UserActions.md#select_entry]
{
    wxControlWithItems* box = dynamic_cast<wxControlWithItems*>(control);
    const int index = box != nullptr ? box->FindString(entry) : wxNOT_FOUND;
    if (index == wxNOT_FOUND)
        return false;
    box->SetSelection(index);
    send_command(*box, dynamic_cast<wxListBox*>(control) != nullptr ? wxEVT_LISTBOX : wxEVT_CHOICE, index);
    return box->GetSelection() == index;
}

bool set_check_box(wxWindow* check_box, bool checked)  // [UserActions.md#set_check_box]
{
    wxCheckBox* box = dynamic_cast<wxCheckBox*>(check_box);
    if (box == nullptr || !box->IsEnabled())
        return false;
    const int box_side = box->GetSize().y;
    if (box->GetValue() != checked && !click_on(*box, wxPoint(box_side / 2, box_side / 2)))  // [UserActions.md#click-the-box]
        return false;
    return wait_until([box, checked] { return box->GetValue() == checked; }, CLICK_TIMEOUT_MILLISECONDS);  // [UserActions.md#click-arrives-later]
}

bool set_spin_value(wxWindow* spin, int value)  // [UserActions.md#set_spin_value]
{
    wxSpinCtrl* control = dynamic_cast<wxSpinCtrl*>(spin);
    if (control == nullptr)
        return false;
    control->SetValue(value);
    send_command(*control, wxEVT_SPINCTRL, control->GetValue());
    return control->GetValue() == value;
}

bool show_notebook_page(wxWindow& dialog, const wxString& page_title)  // [UserActions.md#show_notebook_page]
{
    for (wxWindow* child : dialog.GetChildren()) {
        wxNotebook* notebook = dynamic_cast<wxNotebook*>(child);
        if (notebook == nullptr)
            continue;
        for (size_t page = 0; page < notebook->GetPageCount(); ++page) {
            if (notebook->GetPageText(page) != page_title)
                continue;
            notebook->ChangeSelection(page);
            pump();
            return notebook->GetSelection() == static_cast<int>(page);
        }
    }
    return false;
}

bool click_when_enabled(wxWindow* control, int timeout_milliseconds)
{
    if (control == nullptr)
        return false;
    if (!wait_until([control] { return control->IsEnabled(); }, timeout_milliseconds))  // [UserActions.md#update-ui-needs-idle]
        return false;
    return click_on(control);
}

void activate_menu_item(wxFrame& frame, int menu_item_id)
{
    wxCommandEvent event(wxEVT_MENU, menu_item_id);  // [UserActions.md#activate_menu_item]
    event.SetEventObject(&frame);
    frame.GetEventHandler()->ProcessEvent(event);
}

bool activate_toolbar_tool(wxFrame& frame, int tool_id)  // [UserActions.md#activate_toolbar_tool]
{
    wxToolBar* toolbar = frame.GetToolBar();
    if (toolbar == nullptr || toolbar->FindById(tool_id) == nullptr || !toolbar->GetToolEnabled(tool_id))
        return false;
    wxCommandEvent event(wxEVT_TOOL, tool_id);
    event.SetEventObject(toolbar);
    toolbar->GetEventHandler()->ProcessEvent(event);
    pump();
    return true;
}

namespace {

wxTreeItemId child_named(wxTreeCtrl& tree, wxTreeItemId parent, const wxString& label)
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

wxTreeItemId find_tree_item_for_path(wxTreeCtrl& tree, const std::filesystem::path& path)
{
    wxTreeItemId current = tree.GetRootItem();
    for (const auto& segment : path) {
        if (segment == path.root_directory())
            continue;
        current = child_named(tree, current, segment.wstring());
        if (!current.IsOk())
            return current;
    }
    return current;
}

}
