#pragma once

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/treectrl.h>
#include <filesystem>

namespace guitest {  // [UserActions.md#UserActions]

wxWindow* find(wxWindow& parent, wxWindowID id);
wxString valueOf(wxWindow* control);
bool clickOn(wxWindow* control);
bool clickOn(wxTreeCtrl& tree, wxTreeItemId item);  // [UserActions.md#clickOn-tree-item]
bool typeInto(wxWindow* field, const wxString& text);
bool replaceText(wxWindow* field, const wxString& text);  // [UserActions.md#replaceText]
bool selectRadioOption(wxWindow* radioBox, int option);
bool clickWhenEnabled(wxWindow* control, int timeoutMilliseconds = 5000);
void activateMenuItem(wxFrame& frame, int menuItemId);
wxTreeItemId findTreeItemForPath(wxTreeCtrl& tree, const std::filesystem::path& path);  // [UserActions.md#findTreeItemForPath]

}
