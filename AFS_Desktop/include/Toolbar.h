#pragma once
#include <wx/wxprec.h>
	#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

class Toolbar {
	wxToolBar* toolbar_ = nullptr;
	void replaceTool(int oldToolId, int newToolId, const wxString& label, const wxBitmap& newBitmap, const wxString& shortHelp);
public:
	void create(wxFrame* parent);
	wxToolBar* toolbar();
	void setStarted(bool started);
};

