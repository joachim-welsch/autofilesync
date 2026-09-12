#include "App.h"
#include "id.h"
#include "MainWindow.h"
#include "utilwx.h"
#include <wx/cmdline.h>
#include "wx/fs_zip.h"
#include "wx/generic/helpext.h"
#include "wx/cshelp.h"
#include "Config.h"
#include "Global.h"
#include "Logging.h"
#include "Translation.h"
#include <wx/stdpaths.h>
#include <filesystem>
#ifdef _MACOS
    #include "macutil.h"
#endif


#ifdef _GUITEST  // [App.md#guitest-entry-point]

using namespace AFS_Core;

IMPLEMENT_APP_NO_MAIN(App);
#else
IMPLEMENT_APP(App);
#endif

using utilwx::tul;

namespace {
std::filesystem::path messageCatalogueDir()  // [App.md#messageCatalogueDir]
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxEmptyString);
    for (const wxString& directory : {exe.GetPath(), wxStandardPaths::Get().GetResourcesDir()}) {
        std::filesystem::path candidate = std::filesystem::path(directory.ToStdWstring()) / L"locale";
        if (std::filesystem::is_directory(candidate))
            return candidate;
    }
    return {};
}
}

bool App::OnInit()
{
#ifdef _MACOS
    macTransformToForegroundApp();
#endif
    setlocale(LC_ALL, "de_DE.UTF-8");
    wxLocale locale;
    locale.Init(wxLANGUAGE_GERMAN);
    translation::init(messageCatalogueDir());  // [App.md#language-from-environment]
    if (singleInstanceChecker_.IsAnotherRunning()) {
        log_error(L"Start abgebrochen: eine andere Instanz von AutoFileSync läuft bereits.");  // [App.md#log-single-instance]
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
    if (Config::instance().settings().start_minimized())
		main->Iconize(true);
    else {
        provider->SetHelpController(&main->GetHelpController());
        main->Center();
		main->Show(true);
    }
    Global::instance().set_app(this);
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
    parser.SetSwitchChars(wxT("-"));  // must refuse '/' as a switch starter, or "/path" style paths break
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    consoleMode_ = parser.Found(wxT("console"));
    return true;
}
