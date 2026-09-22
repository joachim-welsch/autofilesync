#pragma once

#include <wx/wxprec.h>
#include <wx/html/helpctrl.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#include <chrono>
#include <optional>
#include <string>
#include "Menu.h"
#include "Toolbar.h"
#include "Global.h"
#include "SyncInterface.h"

class SplitterWindow;
class ListWindow;
class GridWindow;
class Canvas;
class ProgressDialog;
class wxToolBar;
namespace AFS_Core { class Synchronize; class SyncControler; class SyncItem; }
class TaskBarIcon;
class ChatDialog;
class wxHelpControllerBase;
class wxHtmlHelpController;

using namespace AFS_Core;

class MainWindow: public wxFrame {
public:
	MainWindow(wxWindow *parent,
			wxWindowID id,
			const wxString& title,
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_FRAME_STYLE,
			const wxString& name = wxASCII_STR(wxFrameNameStr));
	~MainWindow();

	void startSyncTimer();
	void setTitle();
	void askAndSave();
	void setConsoleMode();
	void registerAutomaticStart(bool);
	void close();
	void createMenu();
	Menu& menu() { return menu_; }
	SyncInterface& syncInterface() { return syncInterface_; }
	TaskBarIcon* taskBarIcon() { return taskBarIcon_; }
	void showLicense();  // [MainWindow.md#showLicense]
	wxHelpControllerBase& GetHelpController() { return *htmlHelpController_; }
private:
	void init();
	void resize();
	void update();
	void createToolBar();
	void createHelp();
    void updateMenu();
    void updateSyncState();  // [MainWindow.md#updateSyncState]
    void createSplitterWindow();
	void createListWindow();
	void createGridWindow();
	void setStatusTextWithTimer(const wxString& text, int milliseconds);
	void restoreSizeAndPosition();
	void createTaskBar();

	void onQuit(wxCommandEvent& event);
	void onAbout(wxCommandEvent& event);

	void onSize(wxSizeEvent& event);
	void onTimer(wxTimerEvent& event);
	void onViewTree(wxCommandEvent&);
    void onViewList(wxCommandEvent&);
    void onViewGrid(wxCommandEvent&);
    void onViewLog(wxCommandEvent&);
    void onViewBackground(wxCommandEvent&);
    void onClose(wxCloseEvent& event);

    void onAddSyncItem(wxCommandEvent&);
    void onAddSshConnection(wxCommandEvent&);
    void onFileOpen(wxCommandEvent&);
    void onFileSave(wxCommandEvent&);
    void onFileSaveAs(wxCommandEvent&);
    void onEditSync(wxCommandEvent& event);
    void onUpdateSyncItemSelected(wxUpdateUIEvent& event);  // [MainWindow.md#onUpdateSyncItemSelected]
    void onEditSshAccounts(wxCommandEvent& event);
    void onEditSettings(wxCommandEvent& event);
    void onEditLogging(wxCommandEvent& event);
    void onDeleteSync(wxCommandEvent&);
    void onStartBackgroundService(wxCommandEvent&);
    void onStopBackgroundService(wxCommandEvent&);
    void onChat(wxCommandEvent&);
    void onMenuOpen(wxMenuEvent& event);
    void updateBackgroundServiceMenu();
    void reportBackgroundService(const std::wstring& error, const wxString& success);  // [MainWindow.md#reportBackgroundService]
    void onStartSingleSync(wxCommandEvent& event);
    void onStartContinuousSync(wxCommandEvent& event);
    void onStopContinuousSync(wxCommandEvent& event);
    void onStatusTimer(wxTimerEvent& event);
    void OnHelp(wxCommandEvent& event);

	template <class T> void deleteWindow(T*&);
	std::optional<SyncItem> findSyncItem();

	Menu            menu_;
	SyncInterface   syncInterface_;
	SplitterWindow* splitterWindow_ = nullptr;
	ListWindow*     listWindow_ = nullptr;
	GridWindow*     gridWindow_ = nullptr;
	wxTimer         syncTimer_;
	wxTimer         statusTimer_;
	Toolbar         toolbar_;
	TaskBarIcon*    taskBarIcon_ = nullptr;
	ChatDialog*     chatDialog_ = nullptr;  // [MainWindow.md#chatDialog_]
	bool            init_ = true;
	wxHtmlHelpController*   htmlHelpController_ = nullptr;

private:
	wxDECLARE_EVENT_TABLE();
};

template <class T> 
void MainWindow::deleteWindow(T*& window)
{
	if (window != nullptr) {
		window->Destroy();
		window = nullptr;
	}
}
