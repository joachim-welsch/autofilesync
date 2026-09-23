/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once

#include <wx/wxprec.h>
#include <wx/cmdline.h>
#include <wx/html/helpctrl.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
    #include <wx/snglinst.h>
#endif
    
class App: public wxApp {
    bool consoleMode_ = false;
    wxSingleInstanceChecker singleInstanceChecker_;
    wxHtmlHelpController    embeddedHtmlHelp_;
    wxHtmlHelpWindow*       embeddedHelpWindow_;

public:
	virtual bool OnInit() wxOVERRIDE;
    virtual int OnExit() wxOVERRIDE;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
};

static const wxCmdLineEntryDesc g_cmdLineDesc[] =
{
    { wxCMD_LINE_SWITCH, "h", "help", "show this help message", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_SWITCH, "c", "console", "start console version", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },

    { wxCMD_LINE_NONE }
};


DECLARE_APP(App);
