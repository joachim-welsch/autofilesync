/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <wx/wxprec.h>
#include <wx/html/helpctrl.h>
#include <wx/aboutdlg.h>

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
#include "SyncProposal.h"

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
	static wxAboutDialogInfo aboutInfo();

	void startSyncTimer();
	void setTitle();  // [MainWindow.md#setTitle]
	bool askAndSave(const char* question, long extra_style = 0);  // [MainWindow.md#askAndSave]
	void setConsoleMode();
	void registerAutomaticStart(bool);
	void close();
	void createMenu();
	Menu& menu() { return menu_; }
	SyncInterface& syncInterface() { return syncInterface_; }
	TaskBarIcon* taskBarIcon() { return taskBarIcon_; }
	wxHelpControllerBase& GetHelpController() { return *htmlHelpController_; }
	void addSyncItemFrom(SyncItem preset);  // [MainWindow.md#addSyncItemFrom]
	void applyProposal(const AFS_Core::sync_proposal::Proposal& proposal);  // [MainWindow.md#applyProposal]
private:
	void resize();
	void update();
	void runAddSyncItemDialog(SyncItem* preset);
	void deleteSyncItem(const std::optional<SyncItem>& found);  // [MainWindow.md#deleteSyncItem]
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
	void setTaskBarIcon();
	void switchLanguage();  // [MainWindow.md#switchLanguage]
	void rebuildMenu();
	void rebuildToolBar();
	void rebuildView();

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
    void onUpdateFileSave(wxUpdateUIEvent& event);  // [MainWindow.md#autosave]
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
	bool            firstTick_ = true;
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
