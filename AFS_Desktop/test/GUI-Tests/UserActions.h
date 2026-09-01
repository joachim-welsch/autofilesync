#pragma once

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/treectrl.h>
#include <filesystem>

// User level actions on top of wxUIActionSimulator: every helper is null safe
// and reports failure instead of aborting, so a test states what went wrong.
namespace guitest {

wxWindow* find(wxWindow& parent, wxWindowID id);
wxString valueOf(wxWindow* control);
bool clickOn(wxWindow* control);
// Clicks the tree item's on-screen position, the way a user would - unlike
// wxTreeCtrl::SelectItem() this goes through the real mouse event handlers
// (TreeCtrl::OnMouseLeftDown and whatever it triggers).
bool clickOn(wxTreeCtrl& tree, wxTreeItemId item);
bool typeInto(wxWindow* field, const wxString& text);
// For a field that already holds a value: selects it first, the way a user
// overwrites an entry instead of appending to it.
bool replaceText(wxWindow* field, const wxString& text);
bool selectRadioOption(wxWindow* radioBox, int option);
bool clickWhenEnabled(wxWindow* control, int timeoutMilliseconds = 5000);
void activateMenuItem(wxFrame& frame, int menuItemId);
// Walks the tree the same way TreeCtrl::addBranch() built it: one child node
// per path segment, the filesystem root itself excluded. Invalid if any
// segment has no matching node.
wxTreeItemId findTreeItemForPath(wxTreeCtrl& tree, const std::filesystem::path& path);

}
