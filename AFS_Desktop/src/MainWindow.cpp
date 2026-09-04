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
#include "BackgroundService.h"
#include "TaskBarIcon.h"
#include "TimedMessageBox.h"
#include "Logging.h"
#include "LogCut.h"
#include "App.h"
#include "util.h"
#include "winutil.h"
#include "wx/filesys.h"
#include "wx/fs_zip.h"
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/filedlg.h>
#include <filesystem>

#ifdef _UNIX
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wwrite-strings"
    #include "../icons/sample_xpm.xpm"
    #pragma GCC diagnostic pop
#endif

using util::tul;
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
	const std::wstring pathName = Config::getRootPathname();
	Config::instance().pathname(pathName);
	Config::instance().read(pathName);
	setTitle();
	restoreSizeAndPosition();
	CreateStatusBar(2);
	createToolBar();
	createListWindow();
	createHelp();
	createTaskBar();
	LogCut()();
	if (Config::instance().settings().startMinimized()) {
		TimedMessageBox dlg(this, "AutoFileSync wurde minimiert gestartet.\n Siehe Ausgeblendete Symbole.", "Info", 3000);
		dlg.ShowModal(); // dlg is stack-allocated: its own destructor cleans it up, Destroy() is only for `new`-allocated windows
	}
	if (Config::syncPossible() && Config::instance().settings().startSyncAtStart()) {
		syncInterface_.setActSyncType(SyncType::Continuous);
		menu_.syncMenu()->Enable(tul(id::StartSync), false);
		menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
		// onStartContinuousSync() (the manual "Start" handler) always pairs
		// this mode switch with toolbar_.setStarted(true); this auto-start
		// path used to skip it, leaving the toolbar showing "Start" while a
		// sync was already running in the background.
		toolbar_.setStarted(true);
	}
	else {
		menu_.syncMenu()->Enable(tul(id::StopSync), false);
	}
	startSyncTimer();
	setStatusTextWithTimer("Willkommen zu AutoFileSync!", 10000);
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

void MainWindow::setTitle()
{
	SetTitle(util::wstring_to_string(L"AutoFileSync - " + Config::instance().pathname()));
}

void MainWindow::startSyncTimer()
{
    LOG_DEBUG(L"MainWindow::startTimer()");
    syncTimer_.Bind(wxEVT_TIMER, &MainWindow::onTimer, this);
    syncTimer_.Start(1000);
}

#ifdef _UNIX
const std::string autostartDir = std::string(getenv("HOME")) + "/.config/autostart/";
const std::string desktopFileName = "my_program.desktop";

// Funktion zum Erstellen der .desktop-Datei
void enableAutostart(const std::string &execPath) {
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
        std::cout << "Autostart aktiviert.\n";
    } else {
        std::cerr << "Konnte die Datei nicht erstellen.\n";
    }
}

// Funktion zum Löschen der .desktop-Datei
void disableAutostart() {
    if (access((autostartDir + desktopFileName).c_str(), F_OK) == 0) {
        if (remove((autostartDir + desktopFileName).c_str()) == 0) {
            std::cout << "Autostart deaktiviert.\n";
        } else {
            std::cerr << "Fehler beim Löschen der Datei.\n";
        }
    } else {
        std::cout << "Autostart war bereits deaktiviert.\n";
    }
}

std::string getExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);

    if (count != -1) {
        return std::string(result, count);
    } else {
        std::cerr << "Fehler beim Abrufen des Pfads: " << strerror(errno) << std::endl;
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

void MainWindow::init()
{
    auto s = GetClientSize();
    s.IncBy(wxPoint(100, 10));
    SetSize(s);
    resize();
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
    wxMessageBox(wxString::Format
                 (
                    "Willkommen zu %s!\n"
                    "Version %s\n"
                    "\n"
                    "Dies ist das automatische Datei-Synchronisationsprogramm\n"
                    "unter %s.",
                    afsNAME_STRING,
                    afsVERSION_STRING,
                    wxGetOsDescription()
                 ),
                 L"Über AutoFileSync",
                 wxOK | wxICON_INFORMATION,
                 this);
}

void MainWindow::onAddSyncItem(wxCommandEvent& WXUNUSED(event))
{
    SyncItemDialog dlg(nullptr, this, wxID_ANY, txt::AddSynchronisation);
    if (dlg.ShowModal() == wxID_OK) {
        // saveData() is the single source of truth for reading the dialog's
        // state (same path onEditSync() uses) - the old manual reconstruction
        // here had no accessor for compareItems/deleteInTarget and silently
        // saved content-only comparison and deleteInTarget=false regardless
        // of what the "Dateivergleich nach:"/"Gelöschte Dateien..." checkboxes
        // actually showed.
        SyncItem syncItem;
        dlg.saveData(syncItem);
        Config& cfg(Config::instance());
        cfg.add(syncItem);
        update();
    }
}

void MainWindow::onAddSshConnection(wxCommandEvent& WXUNUSED(event))
{
    SshAccount sshAccount;
    SshAccountDialog dlg(&sshAccount, this, wxID_ANY, txt::AddSshConnection);
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(sshAccount);
        Config& cfg(Config::instance());
        cfg.add(sshAccount);
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
}

const char* const configWildcard = "Konfigurationsdateien (*.json)|*.json";

// util::pathOnly() only splits at backslashes, so on Unix it discards the whole
// path and the dialog would always start in the home directory.
static wxString configDialogDir(const std::filesystem::path& pathname)
{
    return pathname.has_parent_path() ? wxString(pathname.parent_path().wstring()) : wxGetHomeDir();
}

void MainWindow::onFileOpen(wxCommandEvent&)
{
    askAndSave();
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, L"Konfigurationsdatei öffnen", configDialogDir(current), wxEmptyString,
        configWildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring path = dialog.GetPath().ToStdWstring();
    if (cfg.read(path)) {
        cfg.pathname(path);
        setTitle();
    }
    // read() resets the configuration before it fails, so the previous one is
    // gone from memory: reload it, otherwise "Speichern" would overwrite the
    // still intact previous file with an empty configuration. Skipped for a
    // previous file that never existed, which would only report a second,
    // pointless read error.
    else if (std::filesystem::exists(current))
        cfg.read(current.wstring());
    update();
}

bool save()
{
    Config& cfg(Config::instance());
    if (cfg.write(cfg.pathname()))
        return true;
    // write()'s return value used to be discarded here (and at its other two
    // call sites): a failed save - disk full, permissions - left the user
    // believing "Konfiguration gespeichert" while the on-disk file was
    // untouched (round 4's atomic write correctly refuses to overwrite it on
    // failure), and askAndSave() would then let the app close right over the
    // lost changes.
    wxMessageBox(_("Konfiguration konnte nicht gespeichert werden."), _("Fehler"), wxOK | wxICON_ERROR);
    return false;
}

void MainWindow::setStatusTextWithTimer(const wxString& text, int milliseconds)
{
    SetStatusText(text);
    statusTimer_.Bind(wxEVT_TIMER, &MainWindow::onStatusTimer, this);
    statusTimer_.Start(milliseconds, wxTIMER_ONE_SHOT);
}

void MainWindow::onFileSave(wxCommandEvent& WXUNUSED(event))
{
    if (save())
        setStatusTextWithTimer("Konfiguration gespeichert", 5000);
}

// GTK does not append the filter's extension, so a name typed without one would
// produce a file that the *.json filter hides on the next open.
static wstring withJsonExtension(const wstring& path)
{
    return std::filesystem::path(path).extension() == L".json" ? path : path + L".json";
}

// The dialog's own overwrite prompt only saw the name the user typed; an
// appended extension can still collide with an existing file.
static bool confirmOverwrite(wxWindow* parent, const wstring& path)
{
    if (!std::filesystem::exists(path))
        return true;
    return wxMessageBox(wxString::Format(L"'%s' existiert bereits. Überschreiben?", wxString(path)),
        L"Datei überschreiben", wxYES_NO | wxICON_QUESTION, parent) == wxYES;
}

void MainWindow::onFileSaveAs(wxCommandEvent& WXUNUSED(event))
{
	Config& cfg(Config::instance());
    const std::filesystem::path current(cfg.pathname());
    wxFileDialog dialog(this, L"Name für die Konfigurationsdatei wählen", configDialogDir(current),
        current.filename().wstring(), configWildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const wstring chosen = dialog.GetPath().ToStdWstring();
    const wstring path = withJsonExtension(chosen);
    if (path != chosen && !confirmOverwrite(this, path))
        return;
    if (!cfg.write(path)) {
        wxMessageBox(_("Konfiguration konnte nicht gespeichert werden."), _("Fehler"), wxOK | wxICON_ERROR);
        return;
    }
    cfg.pathname(path);
    setTitle();
    setStatusTextWithTimer("Konfiguration gespeichert", 5000);
}

void MainWindow::onStartSingleSync(wxCommandEvent& WXUNUSED(event))
{
	LOG_DEBUG(L"MainWindow::onStartSingleSync()");
	if (syncInterface_.singleSync()) {
		menu_.syncMenu()->Enable(tul(id::StartSync), false);
		menu_.syncMenu()->Enable(tul(id::SyncOnce), false);
	}
}

void MainWindow::onStartContinuousSync(wxCommandEvent& WXUNUSED(event))
{
	LOG_DEBUG(L"MainWindow::onStartContinuousSync()");  
	syncInterface_.startContinuousSync();
	toolbar_.setStarted(true);
}

void MainWindow::onStopContinuousSync(wxCommandEvent& WXUNUSED(event))
{
	LOG_DEBUG(L"MainWindow::onStopContinuousSync()");
	syncInterface_.stopContinuousSync();
	toolbar_.setStarted(false);
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
	LOG_DEBUG(L"MainWindow::onTimer()");
	if (!Config::exists() || !Config::instance().ready())
		return;
	updateMenu();
	syncInterface_.onTimer();
	if (init_) {
		init_ = false;
		init();
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
            found = Config::instance().findByName(syncName);
    }
    else if (gridWindow_ != nullptr) {
        const wstring syncName = gridWindow_->getSelected();
        if (!syncName.empty())
            found = Config::instance().findByName(syncName);
    }
    return found;
}

void MainWindow::onEditSync(wxCommandEvent&)
{
    auto found = findSyncItem();
    if (found == nullopt) {
        wxMessageBox("Synchronisation konnte nicht gefunden werden.", "Fehler");
        return;
    }
    // found is a copy; write it back under Config's lock via the original name
    // (the dialog may have renamed it).
    const std::wstring originalName = found->name();
    SyncItemDialog dlg(&*found, this, wxID_ANY, txt::EditSynchronisation);
    if (dlg.ShowModal() == wxID_OK) {
        dlg.saveData(*found);
        Config::instance().updateByName(originalName, *found);
        update();
    }
}

void MainWindow::onUpdateSyncItemSelected(wxUpdateUIEvent& event)
{
    auto found = findSyncItem();
    event.Enable(found.has_value());
}

void MainWindow::onEditSshAccounts(wxCommandEvent&)
{
    SshAccountsDialog dlg(this, wxID_ANY, txt::SshAccounts);
    dlg.ShowModal();
}

void MainWindow::onEditSettings(wxCommandEvent&)
{
    bool autoStartLast = Config::instance().settings().autoStart();
    SettingsDialog dlg(this, wxID_ANY, txt::EditSettings);
    dlg.ShowModal();
    if (autoStartLast !=  Config::instance().settings().autoStart())
        registerAutomaticStart(Config::instance().settings().autoStart());
}

void MainWindow::onEditLogging(wxCommandEvent&)
{
    LoggingDialog dlg(this, wxID_ANY, txt::EditLogging);
    if (dlg.ShowModal() == wxID_OK) {
    }
}

void MainWindow::onChat(wxCommandEvent&)
{
    if (chatDialog_ == nullptr)
        chatDialog_ = new ChatDialog(this, wxID_ANY, txt::Chat);
    chatDialog_->Show();
    chatDialog_->Raise();
}

void MainWindow::onDeleteSync(wxCommandEvent&)
{
    auto found = findSyncItem();
    if (found == nullopt) {
        wxMessageBox("Synchronisation konnte nicht gefunden werden.", "Fehler");
        return;
    }
    int answer = wxMessageBox(
        wxString::Format(_(L"Soll die Synchronisation '%s' wirklich gelöscht werden?"), wxString(found->name())),
        _(L"Löschen bestätigen"),
        wxYES_NO | wxICON_QUESTION, this);
    if (answer != wxYES)
        return;
    Config& cfg(Config::instance());
    cfg.eraseByName(found->name());
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

void MainWindow::onViewLog(wxCommandEvent&)
{
    wxString commandLine = Logging::instance().getEditorPathName() + wxString(L" ") + Logging::instance().getLogFilePathName();
    long pid = 0;
    pid = wxExecute(commandLine);
    if (pid == 0) {
		wxMessageBox("Der konfigurierte Editor konnte nicht gestartet werden.", "Fehler");
	}
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
        case BackgroundService::State::Running: return L"Zustand: läuft";
        case BackgroundService::State::Stopped: return L"Zustand: gestoppt";
        case BackgroundService::State::NotInstalled: return L"Zustand: nicht installiert";
        case BackgroundService::State::Unknown: break;
    }
    return L"Zustand: unbekannt";
}
}

// The service can also be started or stopped outside of AutoFileSync, so its
// state is asked for whenever the menu opens rather than being tracked here.
// A state that cannot be determined leaves both commands available: refusing
// them would lock the user out of a service that may well be controllable.
void MainWindow::updateBackgroundServiceMenu()
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
    reportBackgroundService(BackgroundService::start(), "Hintergrunddienst gestartet.");
}

void MainWindow::onStopBackgroundService(wxCommandEvent&)
{
    const std::wstring error = BackgroundService::stop();
    if (error.empty())
        LOG_INFO(L"Hintergrunddienst gestoppt.");
    reportBackgroundService(error, "Hintergrunddienst gestoppt.");
}

void MainWindow::reportBackgroundService(const std::wstring& error, const wxString& success)
{
    if (error.empty())
        setStatusTextWithTimer(success, 10000);
    else
        wxMessageBox(error, "Hintergrunddienst", wxOK | wxICON_ERROR, this);
}

void MainWindow::onQuit(wxCommandEvent& event)
{
    close();
    event.Skip();
}

void MainWindow::onClose(wxCloseEvent& event)
{
    // Save window size and position to wxConfig
    wxConfigBase* config = wxConfig::Get();
    wxPoint pos = GetPosition();
    wxSize size = GetSize();
    config->Write("/MainWindow/x", static_cast<long>(pos.x));
    config->Write("/MainWindow/y", static_cast<long>(pos.y));
    config->Write("/MainWindow/width", static_cast<long>(size.GetWidth()));
    config->Write("/MainWindow/height", static_cast<long>(size.GetHeight()));
    config->Flush();

    if (event.CanVeto() && Config::instance().settings().minimizeInsteadOfClose()) {
        // Keep the app (and any running background sync) alive in the tray
        // instead of closing - same as onViewBackground(). CanVeto() is false
        // for a forced close (tray "Beenden", OS logoff), which must always
        // go through regardless of this setting.
        event.Veto();
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

// Anchoring the search at the executable rather than at the working directory is
// what keeps the help reachable when a desktop launcher, the autostart entry or
// a debugger starts the program from an unrelated directory. An installation
// puts the book next to the executable, and so does every build (CMake post
// build step, Windows post build step); walking up to three directories still
// finds a book placed at the root of a build tree. The macOS bundle keeps it
// among its resources, and the working directory stays a candidate for callers
// that rely on it.
wxString locateHelpBook()
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
        // Names where the book belongs, not the last place searched: that is where a broken installation gets repaired.
        TimedMessageBox dlg(this, "Hilfedatei nicht gefunden: ", helpBookIn(executableDir()), 3000);
        dlg.ShowModal(); // dlg is stack-allocated: its own destructor cleans it up, Destroy() is only for `new`-allocated windows
    }
    else if (!htmlHelpController_->AddBook(helpFilePath))
        wxMessageBox(L"Fehler beim Hinzufügen der Hilfedatei: " + helpFilePath, "Fehler", wxOK | wxICON_ERROR);
    else
        LOG_DEBUG2("Hilfedatei erfolgreich hinzugefügt: ", std::string(helpFilePath.ToUTF8()));
    GetHelpController().SetViewer("internal");
}

void MainWindow::restoreSizeAndPosition() // Restore window size and position from wxConfig
{
    SetMinSize(wxSize(500, 400));
    wxConfigBase* config = wxConfig::Get();
    int x = config->ReadLong("/MainWindow/x", -1);
    int y = config->ReadLong("/MainWindow/y", -1);
    int w = config->ReadLong("/MainWindow/width", 800);
    int h = config->ReadLong("/MainWindow/height", 600);
    if (x >= 0 && y >= 0) {
        SetPosition(wxPoint(x, y));
    }
    SetSize(wxSize(w, h));
}

void MainWindow::createTaskBar()
{
    taskBarIcon_ = new TaskBarIcon(this);
#ifndef __WXOSX__
    // On macOS wxTaskBarIcon addresses the Dock icon, which the application
    // bundle already supplies as afs.icns in every size the Dock asks for.
    // Setting it here would replace that with the single 64x64 XPM.
    if (!taskBarIcon_->SetIcon(wxICON(sample),
        "AutoFileSync\n"
        "Minimiertes Synchronisationsprogramm."))
    {
        wxLogError("Could not set icon.");
    }
#endif
}

void MainWindow::askAndSave()
{
    if (Config::instance().changed()) {
        wxMessageDialog dlg = wxMessageDialog(this,
            wxT("Speichern?"), wxT("Die Daten wurden nicht gespeichert."),
            wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
        if (dlg.ShowModal() == wxID_YES)
            save();
    }
}

void MainWindow::close()
{
    // Always a real shutdown: onClose() already decided to minimize instead
    // and returns before calling this, so getting here must stop sync and
    // actually exit regardless of the "minimize instead of close" setting.
    syncInterface_.stopSync();
    askAndSave();
    wxGetApp().ExitMainLoop();
}
