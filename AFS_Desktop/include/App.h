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
#include <wx/cmdline.h>
#include <wx/html/helpctrl.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
    #include <wx/snglinst.h>
#endif
    
class App: public wxApp {
    bool consoleMode_ = false;
    wxLocale locale_;  // [App.md#wx-locale]
    wxSingleInstanceChecker singleInstanceChecker_;
    wxHtmlHelpController    embeddedHtmlHelp_;
    wxHtmlHelpWindow*       embeddedHelpWindow_;

public:
	virtual bool OnInit() wxOVERRIDE;
    virtual int OnExit() wxOVERRIDE;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
};

DECLARE_APP(App);
