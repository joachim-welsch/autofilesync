#include "UserActions.h"
#include "EventPump.h"
#include <wx/radiobox.h>
#include <wx/textctrl.h>
#include <wx/uiaction.h>
#include <cctype>
#include <string_view>
#ifdef _WIN32
    #include <wx/msw/wrapwin.h>
#elif !defined(_MACOS)
    #include <X11/XKBlib.h>
#endif

namespace guitest {
namespace {

wxPoint centerOnScreen(wxWindow& control)
{
    const wxSize size = control.GetSize();
    return control.GetScreenPosition() + wxPoint(size.x / 2, size.y / 2);
}

// wxUIActionSimulator::Text() accepts letters, digits and " +-.," only, which
// rules out the path separators every test path needs. Char() takes a single
// character, but what it expects there differs per platform.
#ifdef _WIN32
// The character is injected directly instead of through a key of the active
// layout. Going through keys means going through the layout's dead keys: on a
// German keyboard '`' and '^' produce nothing until the following key decides
// what to combine into, so a value ending in one loses it - and the pending
// character then turns up in whatever is typed next, in the next test. The
// layout also decides which modifiers a character needs (AltGr for '[', '\',
// '@' among others), and none of that matters with an injected character.
bool typeCharacter(wxUIActionSimulator&, wxUniChar character)
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
// There is no keyboard map to ask here without pulling in the Carbon text input
// API, and Char() presses no modifier by itself on this port either. The
// shifted set of the standard US layout is therefore hard coded - the tests
// type ASCII only, but on a different layout a character sitting on another key
// would arrive unshifted.
bool typeCharacter(wxUIActionSimulator& simulator, wxUniChar character)
{
    if (!character.IsAscii())
        return false;
    const std::string_view shiftedOnUsLayout = R"(~!@#$%^&*()_+{}|:"<>?)";
    const char ascii = static_cast<char>(character.GetValue());
    const bool needsShift = std::isupper(static_cast<unsigned char>(ascii)) != 0
        || shiftedOnUsLayout.find(ascii) != std::string_view::npos;
    return simulator.Char(character.GetValue(), needsShift ? wxMOD_SHIFT : wxMOD_NONE);
}
#else
// Own connection to the same server the application uses: only the keyboard
// mapping is read, and asking X directly avoids pulling GTK's headers in.
Display* keyboardDisplay()
{
    static Display* display = XOpenDisplay(nullptr);
    return display;
}

// Char() looks the keysym up in the layout but never presses a modifier by
// itself, so every character sitting on a shifted key - '$', '!', ':' and each
// capital - would arrive as the unshifted character of the same key. Which
// characters those are depends on the layout, so the layout decides here too.
bool typeCharacter(wxUIActionSimulator& simulator, wxUniChar character)
{
    if (!character.IsAscii())
        return false;
    Display* display = keyboardDisplay();
    if (display == nullptr)
        return false;
    // ASCII code points and their X keysyms are identical, which is the same
    // mapping Char() applies before it looks for the key.
    const KeySym keysym = character.GetValue();
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
    // Moving the pointer first also settles the keyboard focus: on an X server
    // without a window manager the focus follows the pointer, so a key event
    // sent to a control the pointer never visited would end up elsewhere.
    if (!simulator.MouseMove(centerOnScreen(*control)))
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
    // Only the item's row needs to be visible, not the tree itself scrolled to
    // it: EnsureVisible() before this call is the caller's responsibility.
    if (!tree.GetBoundingRect(item, rect, true))
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
    // Selected after the click, not before: the click sets the caret and drops
    // an earlier selection, so the new text would land in the middle of the old
    // value instead of replacing it.
    textCtrl->SelectAll();
    if (!typeText(text))
        return false;
    return valueOf(field) == text;
}

// wxRadioBox is a single window: on MSW its buttons are internal children that
// wx does not expose, so there is nothing to point the mouse at. Selecting and
// sending the command the click would produce runs the same dialog handler.
bool selectRadioOption(wxWindow* radioBox, int option)
{
    wxRadioBox* box = dynamic_cast<wxRadioBox*>(radioBox);
    if (box == nullptr || option < 0 || option >= static_cast<int>(box->GetCount()))
        return false;
    box->SetSelection(option);
    wxCommandEvent event(wxEVT_RADIOBOX, box->GetId());
    event.SetEventObject(box);
    event.SetInt(option);
    box->GetEventHandler()->ProcessEvent(event);
    pump();
    return box->GetSelection() == option;
}

bool clickWhenEnabled(wxWindow* control, int timeoutMilliseconds)
{
    if (control == nullptr)
        return false;
    // Buttons guarded by EVT_UPDATE_UI only become enabled while the event loop
    // is idle, which is exactly what pumping inside waitUntil() provides.
    if (!waitUntil([control] { return control->IsEnabled(); }, timeoutMilliseconds))
        return false;
    return clickOn(control);
}

void activateMenuItem(wxFrame& frame, int menuItemId)
{
    // wxUIActionSimulator cannot drive a menu bar reliably - the menus are
    // native popup windows whose position wx does not expose on either
    // platform - so the command takes the same event table entry that a real
    // click would reach.
    wxCommandEvent event(wxEVT_MENU, menuItemId);
    event.SetEventObject(&frame);
    frame.GetEventHandler()->ProcessEvent(event);
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
