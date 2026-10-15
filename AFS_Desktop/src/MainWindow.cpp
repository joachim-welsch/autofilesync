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
#include "MainWindow.h"
#include "Version.h"
#include "SplitterWindow.h"
#include "ListWindow.h"
#include "GridWindow.h"
#include "id.h"
#include "utilwx.h"
#include "TextResources.h"
#include "SyncItemDialog.h"
#include "ProgressDialog.h"
#include "SettingsDialog.h"
#include "LoggingDialog.h"
#include "SshAccountDialog.h"
#include "SshAccountsDialog.h"
#include "ChatDialog.h"
#include "Config.h"
#include "Synchronize.h"
#include "SyncControler.h"
#include "SyncInterface.h"
#include "AutoSave.h"
#include "BackgroundService.h"
#include "TaskBarIcon.h"
#include "Logging.h"
#include "LogCut.h"
#include "App.h"
#include "Util.h"
#include "Translation.h"
#ifdef WIN32
    #include <Windows.h> // register_automatic_start() writes the autostart registry key
#endif
#include "wx/filesys.h"
#include "wx/fs_zip.h"
#include <wx/config.h>
#include <wx/display.h>
#include <wx/stdpaths.h>
#include <wx/filedlg.h>
#include <wx/aboutdlg.h>
#include <wx/notifmsg.h>
#ifndef WIN32
    #include <wx/cmdline.h>
    #include <wx/filefn.h>
#endif
#include <algorithm>
#include <filesystem>

#ifdef _UNIX
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wwrite-strings"
    #include "../icons/sample_xpm.xpm"
    #pragma GCC diagnostic pop
#endif

using namespace AFS_Core;

using utilwx::tul;
using std::wstring;
using std::nullopt;

wxDECLARE_APP(App);

namespace {
wxString locate_help_book();
wxString background_service_state_text(BackgroundService::State state);

id menu_id_of(sync_proposal::Command command)
{
    using sync_proposal::Command;
    switch (command) {
        case Command::SyncOnce: return id::SyncOnce;
        case Command::StartSync: return id::StartSync;
        case Command::StopSync: return id::StopSync;
        case Command::StartService: return id::BackgroundServiceStart;
        case Command::StopService: return id::BackgroundServiceStop;
        case Command::OpenLog: return id::View_Log;
    }
    return id::View_Log;
}
}

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_SIZE(MainWindow::on_size)
    EVT_CLOSE(MainWindow::on_close)
    EVT_MENU(tul(id::Quit),  MainWindow::on_quit)
    EVT_MENU(tul(id::About), MainWindow::on_about)
    EVT_MENU(tul(id::SyncOnce), MainWindow::on_start_single_sync)
    EVT_MENU(tul(id::StartSync), MainWindow::on_start_continuous_sync)
    EVT_MENU(tul(id::StopSync), MainWindow::on_stop_continuous_sync)
    EVT_MENU(tul(id::EditSync), MainWindow::on_edit_sync)
    EVT_UPDATE_UI(tul(id::EditSync), MainWindow::on_update_sync_item_selected)
    EVT_MENU(tul(id::EditSettings), MainWindow::on_edit_settings)
    EVT_MENU(tul(id::EditSshAccounts), MainWindow::on_edit_ssh_accounts)
    EVT_MENU(tul(id::EditLogging), MainWindow::on_edit_logging)
    EVT_MENU(tul(id::DeleteSync), MainWindow::on_delete_sync)
    EVT_UPDATE_UI(tul(id::DeleteSync), MainWindow::on_update_sync_item_selected)
    EVT_MENU(tul(id::File_new_Synchronization), MainWindow::on_add_sync_item)
    EVT_MENU(tul(id::File_new_SshConnection), MainWindow::on_add_ssh_connection)
    EVT_MENU(tul(id::File_open), MainWindow::on_file_open)
    EVT_MENU(tul(id::File_save), MainWindow::on_file_save)
    EVT_UPDATE_UI(tul(id::File_save), MainWindow::on_update_file_save)
    EVT_MENU(tul(id::File_save_as), MainWindow::on_file_save_as)
    EVT_MENU(tul(id::View_Tree), MainWindow::on_view_tree)
    EVT_MENU(tul(id::View_List), MainWindow::on_view_list)
    EVT_MENU(tul(id::View_Grid), MainWindow::on_view_grid)
    EVT_MENU(tul(id::View_Log), MainWindow::on_view_log)
    EVT_MENU(tul(id::View_Background), MainWindow::on_view_background)
    EVT_MENU(tul(id::BackgroundServiceStart), MainWindow::on_start_background_service)
    EVT_MENU(tul(id::BackgroundServiceStop), MainWindow::on_stop_background_service)
    EVT_MENU(tul(id::Chat), MainWindow::on_chat)
    EVT_MENU_OPEN(MainWindow::on_menu_open)
    EVT_MENU(wxID_HELP, MainWindow::OnHelp)
wxEND_EVENT_TABLE()

MainWindow::MainWindow(wxWindow *parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		long style,
		const wxString& name):
		wxFrame(parent, id, title, pos, size, style, name),
		sync_interface_(*this, menu_)
{

	SetIcon(wxICON(sample));
	const std::wstring path_name = Config::get_root_pathname();
	Config::instance().pathname(path_name);
	Config::instance().read(path_name);
	translation::set_language(Config::instance().settings().language());  // before the first label is built
	create_menu();
	set_title();
	restore_size_and_position();
	CreateStatusBar(2);
	update_sync_state();
	create_tool_bar();
	create_list_window();
	create_help();
	create_task_bar();
	LogCut()();
	if (Config::instance().settings().start_minimized())
		notify(translation::translate("info"), translation::translate("started_minimized"));
	if (Config::sync_possible() && Config::instance().settings().start_sync_at_start()) {
		sync_interface_.set_act_sync_type(SyncType::Continuous);
		menu_.sync_menu()->Enable(tul(id::StartSync), false);
		menu_.sync_menu()->Enable(tul(id::SyncOnce), false);
		toolbar_.set_started(true);  // [MainWindow.md#autostart-toolbar-state]
	}
	else {
		menu_.sync_menu()->Enable(tul(id::StopSync), false);
	}
	start_sync_timer();
}

MainWindow::~MainWindow()
{
    delete html_help_controller_;
    delete task_bar_icon_;
}

void MainWindow::on_status_timer(wxTimerEvent&)
{
    SetStatusText("");
}

void MainWindow::update_sync_state()
{
    SetStatusText(sync_interface_.state_text(), 1);
}

void MainWindow::set_title()
{
    wstring title = L"AutoFileSync - " + Config::instance().pathname();
    if (Config::instance().changed())
        title += L" " + translation::translate("title_unsaved");
    if (GetTitle() == wxString(title))
        return;
    SetTitle(title);
    OSXSetModified(Config::instance().changed());  // [MainWindow.md#set_title]
    SetRepresentedFilename(Config::instance().pathname());
}

void MainWindow::notify(const wxString& title, const wxString& message)  // [MainWindow.md#notify]
{
    wxNotificationMessage(title, message, this).Show();
}

void MainWindow::start_sync_timer()
{
    log_debug(L"MainWindow::start_timer()");
    sync_timer_.Bind(wxEVT_TIMER, &MainWindow::on_timer, this);
    sync_timer_.Start(1000);
}

#ifdef _UNIX
const std::string AUTOSTART_DIR = std::string(getenv("HOME")) + "/.config/autostart/";
const std::string DESKTOP_FILE_NAME = "my_program.desktop";

void enable_autostart(const std::string &exec_path) {  // [MainWindow.md#enable_autostart]
    std::ofstream desktop_file(AUTOSTART_DIR + DESKTOP_FILE_NAME);

    if (desktop_file.is_open()) {
        desktop_file << "[Desktop Entry]\n";
        desktop_file << "Type=Application\n";
        desktop_file << "Exec=" << exec_path << "\n";
        desktop_file << "Hidden=false\n";
        desktop_file << "NoDisplay=false\n";
        desktop_file << "X-GNOME-Autostart-enabled=true\n";
        desktop_file << "Name=My Program\n";
        desktop_file << "Comment=Start My Program automatically after login\n";
        desktop_file.close();
        std::cout << translation::translate_utf8("autostart_enabled") << "\n";
    } else {
        std::cerr << translation::translate_utf8("autostart_file_not_created") << "\n";
    }
}

void disable_autostart() {  // [MainWindow.md#disable_autostart]
    if (access((AUTOSTART_DIR + DESKTOP_FILE_NAME).c_str(), F_OK) == 0) {
        if (remove((AUTOSTART_DIR + DESKTOP_FILE_NAME).c_str()) == 0) {
            std::cout << translation::translate_utf8("autostart_disabled") << "\n";
        } else {
            std::cerr << translation::translate_utf8("autostart_file_delete_failed") << "\n";
        }
    } else {
        std::cout << translation::translate_utf8("autostart_already_disabled") << "\n";
    }
}

std::string get_executable_path() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);

    if (count != -1) {
        return std::string(result, count);
    } else {
        std::cerr << translation::translate_utf8("executable_path_error") << strerror(errno) << std::endl;
        return std::string();
    }
}
#endif

void MainWindow::register_automatic_start(bool yes)
{
#ifdef WIN32
    HKEY key;
    auto program_name = L"AutoFileSync";
    if (yes) {
        TCHAR path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key);
        RegSetValueEx(key, program_name, 0, REG_SZ, (LPBYTE)path, wcslen(path) * sizeof(TCHAR));
        RegCloseKey(key);
    }
	else {
		RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key);
		RegDeleteValue(key, program_name);
        RegCloseKey(key);
    }
#elif _UNIX
    if (yes)
        enable_autostart(get_executable_path());
    else
        disable_autostart();
#endif
}

void MainWindow::create_splitter_window()
{
    if (splitter_window_ == nullptr) {
        splitter_window_ = new SplitterWindow(this);
        splitter_window_->SetSize(GetClientSize());
        splitter_window_->SetSashGravity(1.0);
        splitter_window_->split_vertically();
        resize();
    }
}

void MainWindow::create_list_window()
{
    if (list_window_ == nullptr) {
        list_window_ = new ListWindow(this);
        resize();
    }
}

void MainWindow::create_grid_window()
{
    if (grid_window_ == nullptr) {
        grid_window_ = new GridWindow(this);
        resize();
    }
}

void MainWindow::create_tool_bar()
{
	toolbar_.create(this);
}

void MainWindow::create_menu()
{
    menu_.create();
    SetMenuBar(menu_.menu_bar()); // attach this menu bar to the frame
}

void MainWindow::resize()
{
    wxSize size = GetClientSize();
    if (splitter_window_ != nullptr) {
        splitter_window_->set_size(0, 0, size.x, size.y);
        splitter_window_->SetSashPosition(size.x / 2);
    }
    else if (list_window_ != nullptr) {
        list_window_->SetSize(0, 0, size.x, size.y); 
    }
    else if (grid_window_ != nullptr) {
        grid_window_->SetSize(0, 0, size.x, size.y);
    }
}

void MainWindow::on_size(wxSizeEvent& event)
{
    if (splitter_window_)
        resize();
    event.Skip();
}

wxAboutDialogInfo MainWindow::about_info()  // [MainWindow.md#on_about]
{
    wxAboutDialogInfo info;
    info.SetName(AFS_NAME_STRING);
    info.SetVersion(AFS_VERSION_STRING);
    info.SetCopyright(translation::translate("about_copyright"));
    info.SetDescription(translation::translate("about_description", {wxGetOsDescription().ToStdWstring()}));
    return info;
}

void MainWindow::on_about(wxCommandEvent& WXUNUSED(event))
{
#ifdef __WXOSX__
    wxAboutBox(about_info(), this);  // [MainWindow.md#on_about]
#else
    wxMessageBox(translation::translate("about_text",
                     {wxString(AFS_NAME_STRING).ToStdWstring(), wxString(AFS_VERSION_STRING).ToStdWstring(), wxGetOsDescription().ToStdWstring()}),
                 translation::translate("about_title"), wxOK | wxICON_INFORMATION, this);
#endif
}

void MainWindow::on_add_sync_item(wxCommandEvent& WXUNUSED(event))
{
    run_add_sync_item_dialog(nullptr);
}

void MainWindow::add_sync_item_from(SyncItem preset)
{
    run_add_sync_item_dialog(&preset);
}

void MainWindow::apply_proposal(const sync_proposal::Proposal& proposal)
{
    if (const auto* item = std::get_if<SyncItem>(&proposal))
        add_sync_item_from(*item);
    else if (const auto* command = std::get_if<sync_proposal::Command>(&proposal))
        run_proposed_command(*command);
    else
        delete_sync_item(Config::instance().find_by_name(std::get<sync_proposal::Deletion>(proposal).name));
}

void MainWindow::run_proposed_command(sync_proposal::Command command)  // [MainWindow.md#run_proposed_command]
{
    const int menu_id = tul(menu_id_of(command));
    update_background_service_menu();
    if (!GetMenuBar()->IsEnabled(menu_id)) {
        wxMessageBox(why_not_possible(command), translation::translate("info"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxCommandEvent event(wxEVT_MENU, menu_id);
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

wxString MainWindow::why_not_possible(sync_proposal::Command command) const
{
    using sync_proposal::Command;
    switch (command) {
        case Command::SyncOnce:
        case Command::StartSync: return translation::translate("sync_already_running");
        case Command::StopSync: return translation::translate("no_sync_active");
        case Command::StartService:
        case Command::StopService:
        case Command::OpenLog: break;
    }
    return background_service_state_text(BackgroundService::state());
}

void MainWindow::run_add_sync_item_dialog(SyncItem* preset)
{
    SyncItemDialog dlg(preset, this, wxID_ANY, txt::add_synchronisation());
    if (dlg.ShowModal() == wxID_OK) {
        SyncItem sync_item;  // [MainWindow.md#save_data-single-source]
        dlg.save_data(sync_item);
        Config& cfg(Config::instance());
        cfg.add(sync_item);
        autosave::after_change(this);  // [MainWindow.md#autosave]
        update();
    }
}

void MainWindow::on_add_ssh_connection(wxCommandEvent& WXUNUSED(event))
{
    SshAccount ssh_account;
    SshAccountDialog dlg(&ssh_account, this, wxID_ANY, txt::add_ssh_connection());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.save_data(ssh_account);
        Config& cfg(Config::instance());
        cfg.add(ssh_account);
        autosave::after_change(this);
    }
}

void MainWindow::update()
{
    if (splitter_window_ != nullptr)
        splitter_window_->update();
    else if (list_window_ != nullptr) {
        list_window_->update();
        resize();
    }
    else if (grid_window_ != nullptr) {
        grid_window_->update();
        resize();
    }
    set_title();
}

static wxString config_wildcard()
{
    return translation::translate("config_file_wildcard");
}

static wxString config_dialog_dir(const std::filesystem::path& pathname)  // [MainWindow.md#config_dialog_dir]
{
    return pathname.has_parent_path() ? wxString(pathname.parent_path().wstring()) : wxGetHomeDir();
}

void MainWindow::on_file_open(wxCommandEvent&)
{
    ask_and_save("save_question");
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, translation::translate("open_config_title"), config_dialog_dir(current), wxEmptyString,
        config_wildcard(), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring path = dialog.GetPath().ToStdWstring();
    if (cfg.read(path)) {
        cfg.pathname(path);
        set_title();
    }
    else if (std::filesystem::exists(current))  // [MainWindow.md#reload-after-failed-read]
        cfg.read(current.wstring());
    update();
}

void MainWindow::set_status_text_with_timer(const wxString& text, int milliseconds)
{
    SetStatusText(text);
    status_timer_.Bind(wxEVT_TIMER, &MainWindow::on_status_timer, this);
    status_timer_.Start(milliseconds, wxTIMER_ONE_SHOT);
}

void MainWindow::on_file_save(wxCommandEvent& WXUNUSED(event))
{
    if (autosave::save(this))
        set_status_text_with_timer(translation::translate("config_saved"), 5000);
    set_title();
}

static wstring with_json_extension(const wstring& path)  // [MainWindow.md#with_json_extension]
{
    return std::filesystem::path(path).extension() == L".json" ? path : path + L".json";
}

static bool confirm_overwrite(wxWindow* parent, const wstring& path)  // [MainWindow.md#confirm_overwrite]
{
    if (!std::filesystem::exists(path))
        return true;
    return utilwx::confirm(parent, translation::translate("overwrite_question", {path}),
        translation::translate("overwrite_title"), translation::translate("button_overwrite"));
}

void MainWindow::on_file_save_as(wxCommandEvent& WXUNUSED(event))
{
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, translation::translate("save_as_title"), config_dialog_dir(current),
        current.filename().wstring(), config_wildcard(), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring chosen = dialog.GetPath().ToStdWstring();
    const wstring path = with_json_extension(chosen);
    if (path != chosen && !confirm_overwrite(this, path))
        return;
    if (!cfg.write(path)) {
        wxMessageBox(translation::translate("config_save_failed"), translation::translate("error"), wxOK | wxICON_ERROR, this);
        return;
    }
    cfg.pathname(path);
    set_title();
    set_status_text_with_timer(translation::translate("config_saved"), 5000);
}

void MainWindow::on_start_single_sync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::on_start_single_sync()");
	if (sync_interface_.single_sync()) {
		menu_.sync_menu()->Enable(tul(id::StartSync), false);
		menu_.sync_menu()->Enable(tul(id::SyncOnce), false);
	}
	update_sync_state();
}

void MainWindow::on_start_continuous_sync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::on_start_continuous_sync()");  
	sync_interface_.start_continuous_sync();
	toolbar_.set_started(true);
	update_sync_state();
}

void MainWindow::on_stop_continuous_sync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::on_stop_continuous_sync()");
	sync_interface_.stop_continuous_sync();
	toolbar_.set_started(false);
	update_sync_state();
}

void MainWindow::set_console_mode()
{
    sync_interface_.set_console_mode();
}

void MainWindow::update_menu()
{
    if (splitter_window_ && splitter_window_->mouse_clicked()) {
        bool found = find_sync_item() != nullopt;
        menu_.edit_menu()->Enable(tul(id::EditSync), found);
    }
}

void MainWindow::on_timer(wxTimerEvent&)
{
	log_debug(L"MainWindow::on_timer()");
	if (!Config::exists() || !Config::instance().ready())
		return;
	update_menu();
	set_title();
	sync_interface_.on_timer();
	update_sync_state();
	if (first_tick_) {
		first_tick_ = false;
		resize();  // the toolbar and status bar have their final height only once the window is shown
	}
}

std::optional<SyncItem> MainWindow::find_sync_item()
{
    std::optional<SyncItem> found = nullopt;
    if (splitter_window_ != nullptr) {
        found = Config::instance().find(splitter_window_->selected_source_path(), true);
        if (found == nullopt) {
            found = Config::instance().find(splitter_window_->selected_target_path(), false);
        }
    }
    else if (list_window_ != nullptr) {
        const wstring sync_name = list_window_->get_selected();
        if (!sync_name.empty())
            found = Config::instance().find_by_name(sync_name);
    }
    else if (grid_window_ != nullptr) {
        const wstring sync_name = grid_window_->get_selected();
        if (!sync_name.empty())
            found = Config::instance().find_by_name(sync_name);
    }
    return found;
}

void MainWindow::on_edit_sync(wxCommandEvent&)
{
    auto found = find_sync_item();
    if (found == nullopt) {
        wxMessageBox(translation::translate("sync_not_found"), translation::translate("error"), wxOK | wxICON_ERROR, this);
        return;
    }
    const std::wstring original_name = found->name();  // [MainWindow.md#copy-written-back]
    SyncItemDialog dlg(&*found, this, wxID_ANY, txt::edit_synchronisation());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.save_data(*found);
        Config::instance().update_by_name(original_name, *found);
        autosave::after_change(this);
        update();
    }
}

void MainWindow::on_update_sync_item_selected(wxUpdateUIEvent& event)
{
    auto found = find_sync_item();
    event.Enable(found.has_value());
}

void MainWindow::on_update_file_save(wxUpdateUIEvent& event)
{
    event.Enable(Config::instance().changed());
}

void MainWindow::on_edit_ssh_accounts(wxCommandEvent&)
{
    SshAccountsDialog dlg(this, wxID_ANY, txt::ssh_connections());
    dlg.ShowModal();
    autosave::after_change(this);
}

void MainWindow::on_edit_settings(wxCommandEvent&)
{
    bool auto_start_last = Config::instance().settings().auto_start();
    const std::string language_last = Config::instance().settings().language();
    SettingsDialog dlg(this, wxID_ANY, txt::settings());
    dlg.ShowModal();
    autosave::after_change(this);
    if (auto_start_last !=  Config::instance().settings().auto_start())
        register_automatic_start(Config::instance().settings().auto_start());
    if (language_last != Config::instance().settings().language())
        switch_language();
}

void MainWindow::switch_language()
{
    translation::set_language(Config::instance().settings().language());
    rebuild_menu();
    rebuild_tool_bar();
    rebuild_view();
    update_sync_state();
    set_title();
    set_task_bar_icon();
}

void MainWindow::rebuild_menu()  // [MainWindow.md#switch_language]
{
    wxMenu* sync = menu_.sync_menu();
    const bool start_enabled = sync->IsEnabled(tul(id::StartSync));
    const bool once_enabled = sync->IsEnabled(tul(id::SyncOnce));
    const bool stop_enabled = sync->IsEnabled(tul(id::StopSync));
    wxMenuBar* old = GetMenuBar();
    create_menu();
    delete old;
    menu_.sync_menu()->Enable(tul(id::StartSync), start_enabled);
    menu_.sync_menu()->Enable(tul(id::SyncOnce), once_enabled);
    menu_.sync_menu()->Enable(tul(id::StopSync), stop_enabled);
}

void MainWindow::rebuild_tool_bar()
{
    wxToolBar* old = GetToolBar();
    const bool started = old->FindById(tul(id::StopSync)) != nullptr;
    SetToolBar(nullptr);
    old->Destroy();
    create_tool_bar();
    toolbar_.set_started(started);
}

void MainWindow::rebuild_view()  // [MainWindow.md#switch_language]
{
    if (list_window_ != nullptr) {
        delete_window(list_window_);
        create_list_window();
    }
    else if (grid_window_ != nullptr) {
        delete_window(grid_window_);
        create_grid_window();
    }
    else if (splitter_window_ != nullptr) {
        delete_window(splitter_window_);
        create_splitter_window();
    }
}

void MainWindow::on_edit_logging(wxCommandEvent&)
{
    LoggingDialog dlg(this, wxID_ANY, txt::logging_settings());
    dlg.ShowModal();
    autosave::after_change(this);
}


wxString MainWindow::chat_situation() const  // [MainWindow.md#chat_situation]
{
    return translation::translate("assistant_situation",
        {sync_interface_.state_text().ToStdWstring(), background_service_state_text(BackgroundService::state()).ToStdWstring()});
}

void MainWindow::on_chat(wxCommandEvent&)
{
    if (chat_dialog_ == nullptr)
        chat_dialog_ = new ChatDialog(this, wxID_ANY, txt::assistant(), [this](const sync_proposal::Proposal& proposal) { apply_proposal(proposal); },
            locate_help_book(), [this] { return chat_situation(); });
    chat_dialog_->Show();
    chat_dialog_->Raise();
}

void MainWindow::on_delete_sync(wxCommandEvent&)
{
    delete_sync_item(find_sync_item());
}

void MainWindow::delete_sync_item(const std::optional<SyncItem>& found)
{
    if (found == nullopt) {
        wxMessageBox(translation::translate("sync_not_found"), translation::translate("error"), wxOK | wxICON_ERROR, this);
        return;
    }
    if (!utilwx::confirm(this, translation::translate("confirm_delete_sync", {found->name()}),
            translation::translate("confirm_delete_title"), translation::translate("button_delete")))
        return;
    Config& cfg(Config::instance());
    cfg.erase_by_name(found->name());
    autosave::after_change(this);
    update();
}

void MainWindow::on_view_tree(wxCommandEvent& )
{
    delete_window(list_window_);
    delete_window(grid_window_);
    create_splitter_window();
}

void MainWindow::on_view_list(wxCommandEvent& )
{
    delete_window(splitter_window_);
    delete_window(grid_window_);
    create_list_window();
}

void MainWindow::on_view_grid(wxCommandEvent&)
{
    delete_window(splitter_window_);
    delete_window(list_window_);
    create_grid_window();
}

#ifndef WIN32
static bool editor_exists(const wxString& command_line)  // [MainWindow.md#editor_exists]
{
    const wxArrayString args = wxCmdLineParser::ConvertStringToArgs(command_line, wxCMD_LINE_SPLIT_UNIX);
    if (args.empty())
        return false;
    wxString found;
    return args[0].Contains('/') ? wxIsExecutable(args[0]) : wxFindFileInPath(&found, wxGetenv("PATH"), args[0]);
}
#endif

static bool start_editor(const wxString& command_line)
{
    const wxLogNull no_wx_error_dialog;  // [MainWindow.md#on_view_log]
#ifdef WIN32
    return wxExecute(command_line) != 0;
#else
    return editor_exists(command_line) && wxExecute(command_line) != 0;
#endif
}

void MainWindow::on_view_log(wxCommandEvent&)
{
    const wxString command_line = Logging::instance().get_editor_path_name() + wxString(L" ") + Logging::instance().get_log_file_path_name();
    if (!start_editor(command_line))
        wxMessageBox(translation::translate("editor_start_failed"), translation::translate("error"), wxOK | wxICON_ERROR, this);
}

void MainWindow::on_view_background(wxCommandEvent&)
{
    Show(false);
}

void MainWindow::on_menu_open(wxMenuEvent& event)
{
    if (event.GetMenu() == menu_.background_service_menu())
        update_background_service_menu();
    event.Skip();
}

namespace {
wxString background_service_state_text(BackgroundService::State state)
{
    switch (state) {
        case BackgroundService::State::Running: return translation::translate("bs_state_running");
        case BackgroundService::State::Stopped: return translation::translate("bs_state_stopped");
        case BackgroundService::State::NotInstalled: return translation::translate("bs_state_not_installed");
        case BackgroundService::State::Unknown: break;
    }
    return translation::translate("bs_state_unknown");
}
}

void MainWindow::update_background_service_menu()  // [MainWindow.md#update_background_service_menu-state]
{
    using State = BackgroundService::State;
    const State state = BackgroundService::state();
    wxMenu* menu = menu_.background_service_menu();
    menu->SetLabel(tul(id::BackgroundServiceState), background_service_state_text(state));
    menu->Enable(tul(id::BackgroundServiceStart), state == State::Stopped || state == State::Unknown);
    menu->Enable(tul(id::BackgroundServiceStop), state == State::Running || state == State::Unknown);
}

void MainWindow::on_start_background_service(wxCommandEvent&)
{
    if (!ask_and_save("bs_save_question", wxCANCEL))  // [MainWindow.md#service-reads-the-saved-file]
        return;
    const std::wstring error = BackgroundService::start();
    if (error.empty())
        log_info(translation::translate("bs_started"));
    report_background_service(error, translation::translate("bs_started"));
}

void MainWindow::on_stop_background_service(wxCommandEvent&)
{
    const std::wstring error = BackgroundService::stop();
    if (error.empty())
        log_info(translation::translate("bs_stopped"));
    report_background_service(error, translation::translate("bs_stopped"));
}

void MainWindow::report_background_service(const std::wstring& error, const wxString& success)
{
    if (error.empty())
        set_status_text_with_timer(success, 10000);
    else
        wxMessageBox(error, translation::translate("bs_caption"), wxOK | wxICON_ERROR, this);
}

void MainWindow::on_quit(wxCommandEvent& event)
{
    close();
    event.Skip();
}

void MainWindow::on_close(wxCloseEvent& event)
{
    wxConfigBase* config = wxConfig::Get();
    wxPoint pos = GetPosition();
    wxSize size = GetSize();
    config->Write("/MainWindow/x", static_cast<long>(pos.x));
    config->Write("/MainWindow/y", static_cast<long>(pos.y));
    config->Write("/MainWindow/width", static_cast<long>(size.GetWidth()));
    config->Write("/MainWindow/height", static_cast<long>(size.GetHeight()));
    config->Flush();

    if (event.CanVeto() && Config::instance().settings().minimize_instead_of_close()) {
        event.Veto();  // [MainWindow.md#minimize-instead-of-close]
        Show(false);
        return;
    }
    close();
    event.Skip();
}

void MainWindow::OnHelp(wxCommandEvent&)
{
    get_help_controller().DisplayContents();
}

namespace {
wxString help_book_in(const wxString& directory)
{
    wxFileName book = wxFileName::DirName(directory);
    book.AppendDir("help");
    book.SetFullName("help.htb");
    return book.GetFullPath();
}

wxString executable_dir()
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxEmptyString);
    return exe.GetPath();
}

wxString locate_help_book()  // [MainWindow.md#locate_help_book]
{
    constexpr int MAX_BUILD_TREE_DEPTH = 3;
    wxFileName dir = wxFileName::DirName(executable_dir());
    for (int level = 0; level <= MAX_BUILD_TREE_DEPTH && dir.GetDirCount() > 0; ++level, dir.RemoveLastDir())
        if (const wxString candidate = help_book_in(dir.GetPath()); wxFileExists(candidate))
            return candidate;
    for (const wxString& directory : {wxStandardPaths::Get().GetResourcesDir(), wxGetCwd()})
        if (const wxString candidate = help_book_in(directory); wxFileExists(candidate))
            return candidate;
    return wxEmptyString;
}
}

void MainWindow::create_help()
{
    wxFileSystem::AddHandler(new wxZipFSHandler);
    html_help_controller_ = new wxHtmlHelpController;
    const wxString help_file_path = locate_help_book();
    if (help_file_path.empty())
        notify(translation::translate("help_file_not_found"), help_book_in(executable_dir()));  // [MainWindow.md#help-path-in-message]
    else if (!html_help_controller_->AddBook(help_file_path))
        wxMessageBox(translation::translate("help_file_add_failed") + help_file_path, translation::translate("error"), wxOK | wxICON_ERROR, this);
    else
        log_debug("Hilfedatei erfolgreich hinzugefügt: ", std::string(help_file_path.ToUTF8()));
    get_help_controller().SetViewer("internal");
}

void MainWindow::restore_size_and_position()  // [MainWindow.md#restore-fits-the-screen]
{
    SetMinSize(FromDIP(wxSize(500, 400)));
    wxConfigBase* config = wxConfig::Get();
    const wxPoint saved(config->ReadLong("/MainWindow/x", -1), config->ReadLong("/MainWindow/y", -1));
    const int display = config->HasEntry("/MainWindow/x") ? wxDisplay::GetFromPoint(saved) : wxNOT_FOUND;
    const wxRect area = wxDisplay(display == wxNOT_FOUND ? 0u : static_cast<unsigned>(display)).GetClientArea();
    const wxSize size(std::min<int>(config->ReadLong("/MainWindow/width", 800), area.width),
                      std::min<int>(config->ReadLong("/MainWindow/height", 600), area.height));
    if (display != wxNOT_FOUND)
        SetPosition(wxPoint(std::min(saved.x, area.GetRight() - size.x + 1), std::min(saved.y, area.GetBottom() - size.y + 1)));
    SetSize(size);
}

void MainWindow::create_task_bar()
{
    task_bar_icon_ = new TaskBarIcon(this);
    set_task_bar_icon();
}

void MainWindow::set_task_bar_icon()
{
#ifndef __WXOSX__
    if (!task_bar_icon_->SetIcon(wxICON(sample), translation::translate("taskbar_tooltip")))  // [MainWindow.md#taskbar-icon-macos]
        wxLogError("%s", translation::translate("icon_set_failed"));
#endif
}

bool MainWindow::ask_and_save(const char* question, long extra_style)
{
    if (!Config::instance().changed())
        return true;
    wxMessageDialog dlg(this, translation::translate(question), translation::translate("not_saved_title"),
        wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION | extra_style);
    dlg.SetYesNoCancelLabels(wxString(translation::translate("button_save")), wxString(translation::translate("button_dont_save")),  // [MainWindow.md#ask_and_save]
        wxString(translation::translate("button_cancel")));
    const int answer = dlg.ShowModal();
    if (answer == wxID_CANCEL)
        return false;
    const bool saved = answer == wxID_NO || autosave::save(this);
    set_title();
    return saved;
}

void MainWindow::close()
{
    sync_interface_.stop_sync();  // [MainWindow.md#real-shutdown]
    ask_and_save("save_question");
    wxGetApp().ExitMainLoop();
}
