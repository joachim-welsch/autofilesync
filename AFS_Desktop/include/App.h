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
