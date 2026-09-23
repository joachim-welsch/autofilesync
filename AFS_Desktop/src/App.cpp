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
    locale_.Init(wxLANGUAGE_DEFAULT);  // [App.md#wx-locale]
    translation::init_locale(translation::DEFAULT_LOCALES);  // [App.md#initLocale]
    translation::init(messageCatalogueDir());  // [App.md#language-from-environment]
    if (singleInstanceChecker_.IsAnotherRunning()) {
        log_error(translation::translate("start_aborted_other_instance"));  // [App.md#log-single-instance]
        wxLogError("%s", translation::translate("already_running"));
		return false;
	}
	if (!wxApp::OnInit())
		return false;
    wxHelpControllerHelpProvider* provider = new wxHelpControllerHelpProvider;
    wxHelpProvider::Set(provider);
    wxFileSystem::AddHandler(new wxZipFSHandler);


	MainWindow *main = new MainWindow(nullptr, tul(id::MAINWINDOW), wxEmptyString);  // the title is set from the configuration path
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
    const std::string help = translation::translate_utf8("cmdline_help");
    const std::string console = translation::translate_utf8("cmdline_console");
    const wxCmdLineEntryDesc descriptions[] = {
        { wxCMD_LINE_SWITCH, "h", "help", help.c_str(), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_SWITCH, "c", "console", console.c_str(), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_NONE }
    };
    parser.SetDesc(descriptions);  // copies the texts, so the locals may go
    parser.SetSwitchChars(wxT("-"));  // must refuse '/' as a switch starter, or "/path" style paths break
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    consoleMode_ = parser.Found(wxT("console"));
    return true;
}
