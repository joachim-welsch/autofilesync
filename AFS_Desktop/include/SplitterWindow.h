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

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#endif

#include "wx/splitter.h"
#include "wx/panel.h"

#include "Canvas.h"

class SplitterWindow: public wxSplitterWindow {
public:
    SplitterWindow(wxFrame *parent);

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

