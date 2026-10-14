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
	static wxAboutDialogInfo about_info();

	void start_sync_timer();
	void set_title();  // [MainWindow.md#set_title]
	void notify(const wxString& title, const wxString& message);  // [MainWindow.md#notify]
	bool ask_and_save(const char* question, long extra_style = 0);  // [MainWindow.md#ask_and_save]
	void set_console_mode();
	void register_automatic_start(bool);
	void close();
	void create_menu();
	Menu& menu() { return menu_; }
	SyncInterface& sync_interface() { return sync_interface_; }
	TaskBarIcon* task_bar_icon() { return task_bar_icon_; }
	wxHelpControllerBase& get_help_controller() { return *html_help_controller_; }
	void add_sync_item_from(SyncItem preset);  // [MainWindow.md#add_sync_item_from]
	void apply_proposal(const AFS_Core::sync_proposal::Proposal& proposal);  // [MainWindow.md#apply_proposal]
private:
	void resize();
	void update();
	void run_add_sync_item_dialog(SyncItem* preset);
	void delete_sync_item(const std::optional<SyncItem>& found);  // [MainWindow.md#delete_sync_item]
	void create_tool_bar();
	void create_help();
    void update_menu();
    void update_sync_state();  // [MainWindow.md#update_sync_state]
    void create_splitter_window();
	void create_list_window();
	void create_grid_window();
	void set_status_text_with_timer(const wxString& text, int milliseconds);
	void restore_size_and_position();
	void create_task_bar();
	void set_task_bar_icon();
	void switch_language();  // [MainWindow.md#switch_language]
	void rebuild_menu();
	void rebuild_tool_bar();
	void rebuild_view();

	void on_quit(wxCommandEvent& event);
	void on_about(wxCommandEvent& event);

	void on_size(wxSizeEvent& event);
	void on_timer(wxTimerEvent& event);
	void on_view_tree(wxCommandEvent&);
    void on_view_list(wxCommandEvent&);
    void on_view_grid(wxCommandEvent&);
    void on_view_log(wxCommandEvent&);
    void on_view_background(wxCommandEvent&);
    void on_close(wxCloseEvent& event);

    void on_add_sync_item(wxCommandEvent&);
    void on_add_ssh_connection(wxCommandEvent&);
    void on_file_open(wxCommandEvent&);
    void on_file_save(wxCommandEvent&);
    void on_file_save_as(wxCommandEvent&);
    void on_edit_sync(wxCommandEvent& event);
    void on_update_sync_item_selected(wxUpdateUIEvent& event);  // [MainWindow.md#on_update_sync_item_selected]
    void on_update_file_save(wxUpdateUIEvent& event);  // [MainWindow.md#autosave]
    void on_edit_ssh_accounts(wxCommandEvent& event);
    void on_edit_settings(wxCommandEvent& event);
    void on_edit_logging(wxCommandEvent& event);
    void on_delete_sync(wxCommandEvent&);
    void on_start_background_service(wxCommandEvent&);
    void on_stop_background_service(wxCommandEvent&);
    void on_chat(wxCommandEvent&);
    void on_menu_open(wxMenuEvent& event);
    void update_background_service_menu();
    void report_background_service(const std::wstring& error, const wxString& success);  // [MainWindow.md#report_background_service]
    void on_start_single_sync(wxCommandEvent& event);
    void on_start_continuous_sync(wxCommandEvent& event);
    void on_stop_continuous_sync(wxCommandEvent& event);
    void on_status_timer(wxTimerEvent& event);
    void OnHelp(wxCommandEvent& event);

	template <class T> void delete_window(T*&);
	std::optional<SyncItem> find_sync_item();

	Menu            menu_;
	SyncInterface   sync_interface_;
	SplitterWindow* splitter_window_ = nullptr;
	ListWindow*     list_window_ = nullptr;
	GridWindow*     grid_window_ = nullptr;
	wxTimer         sync_timer_;
	wxTimer         status_timer_;
	Toolbar         toolbar_;
	TaskBarIcon*    task_bar_icon_ = nullptr;
	ChatDialog*     chat_dialog_ = nullptr;  // [MainWindow.md#chat_dialog_]
	bool            first_tick_ = true;
	wxHtmlHelpController*   html_help_controller_ = nullptr;

private:
	wxDECLARE_EVENT_TABLE();
};

template <class T> 
void MainWindow::delete_window(T*& window)
{
	if (window != nullptr) {
		window->Destroy();
		window = nullptr;
	}
}
