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
#include "TimedMessageBox.h"
#include "Logging.h"
#include "LogCut.h"
#include "App.h"
#include "Util.h"
#include "Translation.h"
#ifdef WIN32
    #include <Windows.h> // registerAutomaticStart() writes the autostart registry key
#endif
#include "wx/filesys.h"
#include "wx/fs_zip.h"
#include <wx/config.h>
#include <wx/display.h>
#include <wx/stdpaths.h>
#include <wx/filedlg.h>
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

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_SIZE(MainWindow::onSize)
    EVT_CLOSE(MainWindow::onClose)
    EVT_MENU(tul(id::Quit),  MainWindow::onQuit)
    EVT_MENU(tul(id::About), MainWindow::onAbout)
    EVT_MENU(tul(id::SyncOnce), MainWindow::onStartSingleSync)
    EVT_MENU(tul(id::StartSync), MainWindow::onStartContinuousSync)
    EVT_MENU(tul(id::StopSync), MainWindow::onStopContinuousSync)
    EVT_MENU(tul(id::EditSync), MainWindow::onEditSync)
    EVT_UPDATE_UI(tul(id::EditSync), MainWindow::onUpdateSyncItemSelected)
    EVT_MENU(tul(id::EditSettings), MainWindow::onEditSettings)
    EVT_MENU(tul(id::EditSshAccounts), MainWindow::onEditSshAccounts)
    EVT_MENU(tul(id::EditLogging), MainWindow::onEditLogging)
    EVT_MENU(tul(id::DeleteSync), MainWindow::onDeleteSync)
    EVT_UPDATE_UI(tul(id::DeleteSync), MainWindow::onUpdateSyncItemSelected)
    EVT_MENU(tul(id::File_new_Synchronization), MainWindow::onAddSyncItem)
    EVT_MENU(tul(id::File_new_SshConnection), MainWindow::onAddSshConnection)
    EVT_MENU(tul(id::File_open), MainWindow::onFileOpen)
    EVT_MENU(tul(id::File_save), MainWindow::onFileSave)
    EVT_UPDATE_UI(tul(id::File_save), MainWindow::onUpdateFileSave)
    EVT_MENU(tul(id::File_save_as), MainWindow::onFileSaveAs)
    EVT_MENU(tul(id::View_Tree), MainWindow::onViewTree)
    EVT_MENU(tul(id::View_List), MainWindow::onViewList)
    EVT_MENU(tul(id::View_Grid), MainWindow::onViewGrid)
    EVT_MENU(tul(id::View_Log), MainWindow::onViewLog)
    EVT_MENU(tul(id::View_Background), MainWindow::onViewBackground)
    EVT_MENU(tul(id::BackgroundServiceStart), MainWindow::onStartBackgroundService)
    EVT_MENU(tul(id::BackgroundServiceStop), MainWindow::onStopBackgroundService)
    EVT_MENU(tul(id::Chat), MainWindow::onChat)
    EVT_MENU_OPEN(MainWindow::onMenuOpen)
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
		syncInterface_(*this, menu_)
{

	SetIcon(wxICON(sample));
	createMenu();
	const std::wstring pathName = Config::get_root_pathname();
	Config::instance().pathname(pathName);
	Config::instance().read(pathName);
	setTitle();
	restoreSizeAndPosition();
	CreateStatusBar(2);
	updateSyncState();
	createToolBar();
	createListWindow();
	createHelp();
	createTaskBar();
	LogCut()();
	if (Config::instance().settings().start_minimized()) {
		TimedMessageBox dlg(this, translation::translate("started_minimized"), translation::translate("info"), 3000);
		dlg.ShowModal(); // dlg is stack-allocated: its own destructor cleans it up, Destroy() is only for `new`-allocated windows
	}
	if (Config::sync_possible() && Config::instance().settings().start_sync_at_start()) {
		syncInterface_.setActSyncType(SyncType::Continuous);
		menu_.syncMenu()->Enable(tul(id::StartSync), false);
		menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
		toolbar_.setStarted(true);  // [MainWindow.md#autostart-toolbar-state]
	}
	else {
		menu_.syncMenu()->Enable(tul(id::StopSync), false);
	}
	startSyncTimer();
}

MainWindow::~MainWindow()
{
    delete htmlHelpController_;
    delete taskBarIcon_;
}

void MainWindow::onStatusTimer(wxTimerEvent&)
{
    SetStatusText("");
}

void MainWindow::updateSyncState()
{
    SetStatusText(syncInterface_.stateText(), 1);
}

void MainWindow::setTitle()
{
    wstring title = L"AutoFileSync - " + Config::instance().pathname();
    if (Config::instance().changed())
        title += L" " + translation::translate("title_unsaved");
    if (GetTitle() != wxString(title))
        SetTitle(title);
}

void MainWindow::startSyncTimer()
{
    log_debug(L"MainWindow::startTimer()");
    syncTimer_.Bind(wxEVT_TIMER, &MainWindow::onTimer, this);
    syncTimer_.Start(1000);
}

#ifdef _UNIX
const std::string autostartDir = std::string(getenv("HOME")) + "/.config/autostart/";
const std::string desktopFileName = "my_program.desktop";

void enableAutostart(const std::string &execPath) {  // [MainWindow.md#enableAutostart]
    std::ofstream desktopFile(autostartDir + desktopFileName);

    if (desktopFile.is_open()) {
        desktopFile << "[Desktop Entry]\n";
        desktopFile << "Type=Application\n";
        desktopFile << "Exec=" << execPath << "\n";
        desktopFile << "Hidden=false\n";
        desktopFile << "NoDisplay=false\n";
        desktopFile << "X-GNOME-Autostart-enabled=true\n";
        desktopFile << "Name=My Program\n";
        desktopFile << "Comment=Start My Program automatically after login\n";
        desktopFile.close();
        std::cout << translation::translate_utf8("autostart_enabled") << "\n";
    } else {
        std::cerr << translation::translate_utf8("autostart_file_not_created") << "\n";
    }
}

void disableAutostart() {  // [MainWindow.md#disableAutostart]
    if (access((autostartDir + desktopFileName).c_str(), F_OK) == 0) {
        if (remove((autostartDir + desktopFileName).c_str()) == 0) {
            std::cout << translation::translate_utf8("autostart_disabled") << "\n";
        } else {
            std::cerr << translation::translate_utf8("autostart_file_delete_failed") << "\n";
        }
    } else {
        std::cout << translation::translate_utf8("autostart_already_disabled") << "\n";
    }
}

std::string getExecutablePath() {
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

void MainWindow::registerAutomaticStart(bool yes)
{
#ifdef WIN32
    HKEY hKey;
    auto programName = L"AutoFileSync";
    if (yes) {
        TCHAR szPath[MAX_PATH];
        GetModuleFileName(NULL, szPath, MAX_PATH);
        RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
        RegSetValueEx(hKey, programName, 0, REG_SZ, (LPBYTE)szPath, wcslen(szPath) * sizeof(TCHAR));
        RegCloseKey(hKey);
    }
	else {
		RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
		RegDeleteValue(hKey, programName);
        RegCloseKey(hKey);
    }
#elif _UNIX
    if (yes)
        enableAutostart(getExecutablePath());
    else
        disableAutostart();
#endif
}

void MainWindow::createSplitterWindow()
{
    if (splitterWindow_ == nullptr) {
        splitterWindow_ = new SplitterWindow(this);
        splitterWindow_->SetSize(GetClientSize());
        splitterWindow_->SetSashGravity(1.0);
        splitterWindow_->splitVertically();
        resize();
    }
}

void MainWindow::createListWindow()
{
    if (listWindow_ == nullptr) {
        listWindow_ = new ListWindow(this);
        resize();
    }
}

void MainWindow::createGridWindow()
{
    if (gridWindow_ == nullptr) {
        gridWindow_ = new GridWindow(this);
        resize();
    }
}

void MainWindow::createToolBar()
{
	toolbar_.create(this);
}

void MainWindow::createMenu()
{
    menu_.create();
    SetMenuBar(menu_.menuBar()); // attach this menu bar to the frame
}

void MainWindow::resize()
{
    wxSize size = GetClientSize();
    if (splitterWindow_ != nullptr) {
        splitterWindow_->setSize(0, 0, size.x, size.y);
        splitterWindow_->SetSashPosition(size.x / 2);
    }
    else if (listWindow_ != nullptr) {
        listWindow_->SetSize(0, 0, size.x, size.y); 
    }
    else if (gridWindow_ != nullptr) {
        gridWindow_->SetSize(0, 0, size.x, size.y);
    }
}

void MainWindow::onSize(wxSizeEvent& event)
{
    if (splitterWindow_)
        resize();
    event.Skip();
}

void MainWindow::onAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(translation::translate("about_text",  // [MainWindow.md#onAbout]
                     {wxString(afsNAME_STRING).ToStdWstring(), wxString(afsVERSION_STRING).ToStdWstring(), wxGetOsDescription().ToStdWstring()}),
                 translation::translate("about_title"), wxOK | wxICON_INFORMATION, this);
}

void MainWindow::onAddSyncItem(wxCommandEvent& WXUNUSED(event))
{
    SyncItemDialog dlg(nullptr, this, wxID_ANY, txt::add_synchronisation());
    if (dlg.ShowModal() == wxID_OK) {
        SyncItem syncItem;  // [MainWindow.md#saveData-single-source]
        dlg.saveData(syncItem);
        Config& cfg(Config::instance());
        cfg.add(syncItem);
        autosave::after_change(this);  // [MainWindow.md#autosave]
        update();
    }
}

void MainWindow::onAddSshConnection(wxCommandEvent& WXUNUSED(event))
{
    SshAccount sshAccount;
    SshAccountDialog dlg(&sshAccount, this, wxID_ANY, txt::add_ssh_connection());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(sshAccount);
        Config& cfg(Config::instance());
        cfg.add(sshAccount);
        autosave::after_change(this);
    }
}

void MainWindow::update()
{
    if (splitterWindow_ != nullptr)
        splitterWindow_->update();
    else if (listWindow_ != nullptr) {
        listWindow_->update();
        resize();
    }
    else if (gridWindow_ != nullptr) {
        gridWindow_->update();
        resize();
    }
    setTitle();
}

static wxString configWildcard()
{
    return translation::translate("config_file_wildcard");
}

static wxString configDialogDir(const std::filesystem::path& pathname)  // [MainWindow.md#configDialogDir]
{
    return pathname.has_parent_path() ? wxString(pathname.parent_path().wstring()) : wxGetHomeDir();
}

void MainWindow::onFileOpen(wxCommandEvent&)
{
    askAndSave("save_question");
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, translation::translate("open_config_title"), configDialogDir(current), wxEmptyString,
        configWildcard(), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring path = dialog.GetPath().ToStdWstring();
    if (cfg.read(path)) {
        cfg.pathname(path);
        setTitle();
    }
    else if (std::filesystem::exists(current))  // [MainWindow.md#reload-after-failed-read]
        cfg.read(current.wstring());
    update();
}

void MainWindow::setStatusTextWithTimer(const wxString& text, int milliseconds)
{
    SetStatusText(text);
    statusTimer_.Bind(wxEVT_TIMER, &MainWindow::onStatusTimer, this);
    statusTimer_.Start(milliseconds, wxTIMER_ONE_SHOT);
}

void MainWindow::onFileSave(wxCommandEvent& WXUNUSED(event))
{
    if (autosave::save(this))
        setStatusTextWithTimer(translation::translate("config_saved"), 5000);
    setTitle();
}

static wstring withJsonExtension(const wstring& path)  // [MainWindow.md#withJsonExtension]
{
    return std::filesystem::path(path).extension() == L".json" ? path : path + L".json";
}

static bool confirmOverwrite(wxWindow* parent, const wstring& path)  // [MainWindow.md#confirmOverwrite]
{
    if (!std::filesystem::exists(path))
        return true;
    return wxMessageBox(translation::translate("overwrite_question", {path}),
        translation::translate("overwrite_title"), wxYES_NO | wxICON_QUESTION, parent) == wxYES;
}

void MainWindow::onFileSaveAs(wxCommandEvent& WXUNUSED(event))
{
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, translation::translate("save_as_title"), configDialogDir(current),
        current.filename().wstring(), configWildcard(), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring chosen = dialog.GetPath().ToStdWstring();
    const wstring path = withJsonExtension(chosen);
    if (path != chosen && !confirmOverwrite(this, path))
        return;
    if (!cfg.write(path)) {
        wxMessageBox(translation::translate("config_save_failed"), translation::translate("error"), wxOK | wxICON_ERROR);
        return;
    }
    cfg.pathname(path);
    setTitle();
    setStatusTextWithTimer(translation::translate("config_saved"), 5000);
}

void MainWindow::onStartSingleSync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::onStartSingleSync()");
	if (syncInterface_.singleSync()) {
		menu_.syncMenu()->Enable(tul(id::StartSync), false);
		menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
	}
	updateSyncState();
}

void MainWindow::onStartContinuousSync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::onStartContinuousSync()");  
	syncInterface_.startContinuousSync();
	toolbar_.setStarted(true);
	updateSyncState();
}

void MainWindow::onStopContinuousSync(wxCommandEvent& WXUNUSED(event))
{
	log_debug(L"MainWindow::onStopContinuousSync()");
	syncInterface_.stopContinuousSync();
	toolbar_.setStarted(false);
	updateSyncState();
}

void MainWindow::setConsoleMode()
{
    syncInterface_.setConsoleMode();
}

void MainWindow::updateMenu()
{
    if (splitterWindow_ && splitterWindow_->mouseClicked()) {
        bool found = findSyncItem() != nullopt;
        menu_.editMenu()->Enable(tul(id::EditSync), found);
    }
}

void MainWindow::onTimer(wxTimerEvent&)
{
	log_debug(L"MainWindow::onTimer()");
	if (!Config::exists() || !Config::instance().ready())
		return;
	updateMenu();
	setTitle();
	syncInterface_.onTimer();
	updateSyncState();
	if (firstTick_) {
		firstTick_ = false;
		resize();  // the toolbar and status bar have their final height only once the window is shown
	}
}

std::optional<SyncItem> MainWindow::findSyncItem()
{
    std::optional<SyncItem> found = nullopt;
    if (splitterWindow_ != nullptr) {
        found = Config::instance().find(splitterWindow_->selectedSourcePath(), true);
        if (found == nullopt) {
            found = Config::instance().find(splitterWindow_->selectedTargetPath(), false);
        }
    }
    else if (listWindow_ != nullptr) {
        const wstring syncName = listWindow_->getSelected();
        if (!syncName.empty())
            found = Config::instance().find_by_name(syncName);
    }
    else if (gridWindow_ != nullptr) {
        const wstring syncName = gridWindow_->getSelected();
        if (!syncName.empty())
            found = Config::instance().find_by_name(syncName);
    }
    return found;
}

void MainWindow::onEditSync(wxCommandEvent&)
{
    auto found = findSyncItem();
    if (found == nullopt) {
        wxMessageBox(translation::translate("sync_not_found"), translation::translate("error"));
        return;
    }
    const std::wstring originalName = found->name();  // [MainWindow.md#copy-written-back]
    SyncItemDialog dlg(&*found, this, wxID_ANY, txt::edit_synchronisation());
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(*found);
        Config::instance().update_by_name(originalName, *found);
        autosave::after_change(this);
        update();
    }
}

void MainWindow::onUpdateSyncItemSelected(wxUpdateUIEvent& event)
{
    auto found = findSyncItem();
    event.Enable(found.has_value());
}

void MainWindow::onUpdateFileSave(wxUpdateUIEvent& event)
{
    event.Enable(Config::instance().changed());
}

void MainWindow::onEditSshAccounts(wxCommandEvent&)
{
    SshAccountsDialog dlg(this, wxID_ANY, txt::ssh_connections());
    dlg.ShowModal();
    autosave::after_change(this);
}

void MainWindow::onEditSettings(wxCommandEvent&)
{
    bool autoStartLast = Config::instance().settings().auto_start();
    SettingsDialog dlg(this, wxID_ANY, txt::settings());
    dlg.ShowModal();
    autosave::after_change(this);
    if (autoStartLast !=  Config::instance().settings().auto_start())
        registerAutomaticStart(Config::instance().settings().auto_start());
}

void MainWindow::onEditLogging(wxCommandEvent&)
{
    LoggingDialog dlg(this, wxID_ANY, txt::logging_settings());
    dlg.ShowModal();
    autosave::after_change(this);
}

void MainWindow::onChat(wxCommandEvent&)
{
    if (chatDialog_ == nullptr)
        chatDialog_ = new ChatDialog(this, wxID_ANY, txt::assistant());
    chatDialog_->Show();
    chatDialog_->Raise();
}

void MainWindow::onDeleteSync(wxCommandEvent&)
{
    auto found = findSyncItem();
    if (found == nullopt) {
        wxMessageBox(translation::translate("sync_not_found"), translation::translate("error"));
        return;
    }
    int answer = wxMessageBox(translation::translate("confirm_delete_sync", {found->name()}),
        translation::translate("confirm_delete_title"), wxYES_NO | wxICON_QUESTION, this);
    if (answer != wxYES)
        return;
    Config& cfg(Config::instance());
    cfg.erase_by_name(found->name());
    autosave::after_change(this);
    update();
}

void MainWindow::onViewTree(wxCommandEvent& )
{
    deleteWindow(listWindow_);
    deleteWindow(gridWindow_);
    createSplitterWindow();
}

void MainWindow::onViewList(wxCommandEvent& )
{
    deleteWindow(splitterWindow_);
    deleteWindow(gridWindow_);
    createListWindow();
}

void MainWindow::onViewGrid(wxCommandEvent&)
{
    deleteWindow(splitterWindow_);
    deleteWindow(listWindow_);
    createGridWindow();
}

#ifndef WIN32
static bool editorExists(const wxString& commandLine)  // [MainWindow.md#editorExists]
{
    const wxArrayString args = wxCmdLineParser::ConvertStringToArgs(commandLine, wxCMD_LINE_SPLIT_UNIX);
    if (args.empty())
        return false;
    wxString found;
    return args[0].Contains('/') ? wxIsExecutable(args[0]) : wxFindFileInPath(&found, wxGetenv("PATH"), args[0]);
}
#endif

static bool startEditor(const wxString& commandLine)
{
    const wxLogNull noWxErrorDialog;  // [MainWindow.md#onViewLog]
#ifdef WIN32
    return wxExecute(commandLine) != 0;
#else
    return editorExists(commandLine) && wxExecute(commandLine) != 0;
#endif
}

void MainWindow::onViewLog(wxCommandEvent&)
{
    const wxString commandLine = Logging::instance().get_editor_path_name() + wxString(L" ") + Logging::instance().get_log_file_path_name();
    if (!startEditor(commandLine))
        wxMessageBox(translation::translate("editor_start_failed"), translation::translate("error"));
}

void MainWindow::onViewBackground(wxCommandEvent&)
{
    Show(false);
}

void MainWindow::onMenuOpen(wxMenuEvent& event)
{
    if (event.GetMenu() == menu_.backgroundServiceMenu())
        updateBackgroundServiceMenu();
    event.Skip();
}

namespace {
wxString backgroundServiceStateText(BackgroundService::State state)
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

void MainWindow::updateBackgroundServiceMenu()  // [MainWindow.md#updateBackgroundServiceMenu-state]
{
    using State = BackgroundService::State;
    const State state = BackgroundService::state();
    wxMenu* menu = menu_.backgroundServiceMenu();
    menu->SetLabel(tul(id::BackgroundServiceState), backgroundServiceStateText(state));
    menu->Enable(tul(id::BackgroundServiceStart), state == State::Stopped || state == State::Unknown);
    menu->Enable(tul(id::BackgroundServiceStop), state == State::Running || state == State::Unknown);
}

void MainWindow::onStartBackgroundService(wxCommandEvent&)
{
    if (!askAndSave("bs_save_question", wxCANCEL))  // [MainWindow.md#service-reads-the-saved-file]
        return;
    const std::wstring error = BackgroundService::start();
    if (error.empty())
        log_info(translation::translate("bs_started"));
    reportBackgroundService(error, translation::translate("bs_started"));
}

void MainWindow::onStopBackgroundService(wxCommandEvent&)
{
    const std::wstring error = BackgroundService::stop();
    if (error.empty())
        log_info(translation::translate("bs_stopped"));
    reportBackgroundService(error, translation::translate("bs_stopped"));
}

void MainWindow::reportBackgroundService(const std::wstring& error, const wxString& success)
{
    if (error.empty())
        setStatusTextWithTimer(success, 10000);
    else
        wxMessageBox(error, translation::translate("bs_caption"), wxOK | wxICON_ERROR, this);
}

void MainWindow::onQuit(wxCommandEvent& event)
{
    close();
    event.Skip();
}

void MainWindow::onClose(wxCloseEvent& event)
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
    GetHelpController().DisplayContents();
}

namespace {
wxString helpBookIn(const wxString& directory)
{
    wxFileName book = wxFileName::DirName(directory);
    book.AppendDir("help");
    book.SetFullName("help.htb");
    return book.GetFullPath();
}

wxString executableDir()
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxEmptyString);
    return exe.GetPath();
}

wxString locateHelpBook()  // [MainWindow.md#locateHelpBook]
{
    constexpr int maxBuildTreeDepth = 3;
    wxFileName dir = wxFileName::DirName(executableDir());
    for (int level = 0; level <= maxBuildTreeDepth && dir.GetDirCount() > 0; ++level, dir.RemoveLastDir())
        if (const wxString candidate = helpBookIn(dir.GetPath()); wxFileExists(candidate))
            return candidate;
    for (const wxString& directory : {wxStandardPaths::Get().GetResourcesDir(), wxGetCwd()})
        if (const wxString candidate = helpBookIn(directory); wxFileExists(candidate))
            return candidate;
    return wxEmptyString;
}
}

void MainWindow::createHelp()
{
    wxFileSystem::AddHandler(new wxZipFSHandler);
    htmlHelpController_ = new wxHtmlHelpController;
    const wxString helpFilePath = locateHelpBook();
    if (helpFilePath.empty()) {
        TimedMessageBox dlg(this, translation::translate("help_file_not_found"), helpBookIn(executableDir()), 3000);  // [MainWindow.md#help-path-in-message]
        dlg.ShowModal(); // dlg is stack-allocated: its own destructor cleans it up, Destroy() is only for `new`-allocated windows
    }
    else if (!htmlHelpController_->AddBook(helpFilePath))
        wxMessageBox(translation::translate("help_file_add_failed") + helpFilePath, translation::translate("error"), wxOK | wxICON_ERROR);
    else
        log_debug("Hilfedatei erfolgreich hinzugefügt: ", std::string(helpFilePath.ToUTF8()));
    GetHelpController().SetViewer("internal");
}

void MainWindow::restoreSizeAndPosition()  // [MainWindow.md#restore-fits-the-screen]
{
    SetMinSize(wxSize(500, 400));
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

void MainWindow::createTaskBar()
{
    taskBarIcon_ = new TaskBarIcon(this);
#ifndef __WXOSX__
    if (!taskBarIcon_->SetIcon(wxICON(sample), translation::translate("taskbar_tooltip")))  // [MainWindow.md#taskbar-icon-macos]
        wxLogError("%s", translation::translate("icon_set_failed"));
#endif
}

bool MainWindow::askAndSave(const char* question, long extra_style)
{
    if (!Config::instance().changed())
        return true;
    wxMessageDialog dlg(this, translation::translate(question), translation::translate("not_saved_title"),
        wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION | extra_style);
    const int answer = dlg.ShowModal();
    if (answer == wxID_CANCEL)
        return false;
    const bool saved = answer == wxID_NO || autosave::save(this);
    setTitle();
    return saved;
}

void MainWindow::close()
{
    syncInterface_.stopSync();  // [MainWindow.md#real-shutdown]
    askAndSave("save_question");
    wxGetApp().ExitMainLoop();
}
