#pragma once
#include "Canvas.h"

class TargetCanvas: public Canvas {
public:
	TargetCanvas(wxWindow* parent);
	void update() override;
	void onSelectRoot() override;
	bool source() const override { return false; }
protected:
	void addDataItems() override;
};

