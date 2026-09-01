#pragma once

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#endif

#include "wx/splitter.h"
#include "wx/panel.h"

#include "Canvas.h"

class SplitterWindow: public wxSplitterWindow {
public:
    SplitterWindow(wxFrame *parent);

    // event handlers
    void OnPositionChanged(wxSplitterEvent& event);
    void OnPositionChanging(wxSplitterEvent& event);
    void OnDClick(wxSplitterEvent& event);
    void OnUnsplitEvent(wxSplitterEvent& event);

    void splitVertically();
    void onSelectRoot();
    void update();
    void setSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);
    std::wstring selectedSourcePath() const;
    std::wstring selectedTargetPath() const;
    bool mouseClicked() const;
private:
    wxFrame* parent_;
    Canvas* left_ = nullptr;
    Canvas* right_ = nullptr;
    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(SplitterWindow);
};

