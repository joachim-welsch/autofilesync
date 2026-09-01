#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class Menu {
	wxMenu*		editMenu_ = nullptr;
	wxMenu*		viewMenu_ = nullptr;
	wxMenu*		syncMenu_ = nullptr;
	wxMenu*		backgroundServiceMenu_ = nullptr;
	wxMenuBar*	menuBar_ = nullptr;

	wxMenu* createFileMenu();
	wxMenu* createEditMenu();
	wxMenu* createViewMenu();
	wxMenu* createSyncMenu();
	wxMenu* createBackgroundServiceMenu();
	wxMenu* createAssistantMenu();
	wxMenu* createHelpMenu();
public:
	void create();
	wxMenuBar* menuBar();
	wxMenu* editMenu();
	wxMenu* viewMenu();
	wxMenu* syncMenu();
	wxMenu* backgroundServiceMenu();
	~Menu();
};

