#pragma once

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/grid.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <filesystem>

namespace guitest {  // [UserActions.md#UserActions]

wxWindow* find(wxWindow& parent, wxWindowID id);
wxString valueOf(wxWindow* control);
bool clickOn(wxWindow* control);
bool clickOn(wxTreeCtrl& tree, wxTreeItemId item);  // [UserActions.md#clickOn-tree-item]
bool clickOn(wxListCtrl& list, long item, bool doubleClick = false);  // [UserActions.md#clickOn-list-item]
bool clickOnRowLabel(wxGrid& grid, int row);  // [UserActions.md#clickOnRowLabel]
bool changeGridCell(wxGrid& grid, int row, int col, const wxString& value);  // [UserActions.md#changeGridCell]
bool typeInto(wxWindow* field, const wxString& text);
bool replaceText(wxWindow* field, const wxString& text);  // [UserActions.md#replaceText]
bool replaceLines(wxWindow* field, const wxString& lines);  // [UserActions.md#replaceLines]
bool selectRadioOption(wxWindow* radioBox, int option);
bool selectEntry(wxWindow* control, const wxString& entry);  // [UserActions.md#selectEntry]
bool setCheckBox(wxWindow* checkBox, bool checked);  // [UserActions.md#setCheckBox]
bool setSpinValue(wxWindow* spin, int value);  // [UserActions.md#setSpinValue]
bool showNotebookPage(wxWindow& dialog, const wxString& pageTitle);  // [UserActions.md#showNotebookPage]
bool clickWhenEnabled(wxWindow* control, int timeoutMilliseconds = 5000);
void activateMenuItem(wxFrame& frame, int menuItemId);
bool activateToolbarTool(wxFrame& frame, int toolId);  // [UserActions.md#activateToolbarTool]
wxTreeItemId findTreeItemForPath(wxTreeCtrl& tree, const std::filesystem::path& path);  // [UserActions.md#findTreeItemForPath]

}
