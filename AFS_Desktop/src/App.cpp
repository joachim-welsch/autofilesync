#include "App.h"
#include "id.h"
#include "MainWindow.h"
#include "utilwx.h"
#include <wx/cmdline.h>
#include "wx/fs_zip.h"
#include "wx/generic/helpext.h"
#include "wx/cshelp.h"
#include "config.h"
#include "Global.h"
#include "Logging.h"
#ifdef _MACOS
    #include "macutil.h"
#endif


#ifdef _GUITEST
// The GUI test binary brings its own main() (Google Test); apart from the entry
// point the application under test is built exactly like the shipped one.
IMPLEMENT_APP_NO_MAIN(App);
#else
IMPLEMENT_APP(App);
#endif

using util::tul;

bool App::OnInit()
{
#ifdef _MACOS
    macTransformToForegroundApp();
#endif
    setlocale(LC_ALL, "de_DE.UTF-8");
    wxLocale locale;
    locale.Init(wxLANGUAGE_GERMAN);
    if (singleInstanceChecker_.IsAnotherRunning()) {
        // wxLogError alone is easy to miss headlessly (a cron/systemd
        // --console trigger with no display attached) - log it to the app's
        // own log too, now that the log path actually resolves to somewhere
        // real by default (see the Logging default-path fix).
        LOG_ERROR(L"Start abgebrochen: eine andere Instanz von AutoFileSync läuft bereits.")
        wxLogError(_(L"AutoFileSync läuft bereits."));
		return false;
	}
	if (!wxApp::OnInit())
		return false;
    wxHelpControllerHelpProvider* provider = new wxHelpControllerHelpProvider;
    wxHelpProvider::Set(provider);
    wxFileSystem::AddHandler(new wxZipFSHandler);


	MainWindow *main = new MainWindow(nullptr, tul(id::MAINWINDOW), _("Main Window"));
	if (consoleMode_) {
		main->setConsoleMode();
		main->syncInterface().runConsoleSync();
		main->Close(true);
		return true;
	}
    if (Config::instance().settings().startMinimized())
		main->Iconize(true);
    else {
        provider->SetHelpController(&main->GetHelpController());
        main->Center();
		main->Show(true);
    }
    Global::instance().setApp(this);
	return true;
}

int App::OnExit()
{
    Config::instance().unload();
    Logging::shutdown();
    delete wxHelpProvider::Set(NULL);
    return 0;
}

void App::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc(g_cmdLineDesc);
    // must refuse '/' as parameter starter or cannot use "/path" style paths
    parser.SetSwitchChars(wxT("-"));
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    consoleMode_ = parser.Found(wxT("console"));
    return true;
}
