#pragma once

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
	#include <wx/treebase.h>
    #include <wx/treectrl.h>
#endif

class TreeItemData: public wxTreeItemData {
	wxString description_;
public:
	TreeItemData(const wxString& desc) : description_(desc) { }
	void ShowInfo(wxTreeCtrl* tree);
	wxString const& GetDesc() const { return description_; }
};

