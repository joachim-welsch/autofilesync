#pragma once
#include "Canvas.h"

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#include <wx/treebase.h>
#endif

class SourceCanvas: public Canvas {
public:
	SourceCanvas(wxWindow* parent);
	void update() override;
	void onSelectRoot() override;
	bool source() const override { return true;  }
protected:
	void addDataItems() override;
};
