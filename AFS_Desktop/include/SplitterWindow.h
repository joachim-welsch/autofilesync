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

