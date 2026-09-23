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
#include "SplitterWindow.h"

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/dcmirror.h"
#endif

#include "Util.h"
#include "SourceCanvas.h"
#include "TargetCanvas.h"

wxBEGIN_EVENT_TABLE(SplitterWindow, wxSplitterWindow)
    EVT_SPLITTER_SASH_POS_CHANGED(wxID_ANY, SplitterWindow::OnPositionChanged)
    EVT_SPLITTER_SASH_POS_CHANGING(wxID_ANY, SplitterWindow::OnPositionChanging)

    EVT_SPLITTER_DCLICK(wxID_ANY, SplitterWindow::OnDClick)

    EVT_SPLITTER_UNSPLIT(wxID_ANY, SplitterWindow::OnUnsplitEvent)
wxEND_EVENT_TABLE()

SplitterWindow::SplitterWindow(wxFrame *parent)
                : wxSplitterWindow(parent, wxID_ANY,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_3D | wxSP_LIVE_UPDATE |
                                   wxCLIP_CHILDREN /* | wxSP_NO_XP_THEME */ )
{
    parent_ = parent;
    left_ = new SourceCanvas(this);
    left_->addDataItems();
    right_ = new TargetCanvas(this);
    right_->other(left_);
    left_->other(right_);
    right_->addDataItems();
}

void SplitterWindow::OnPositionChanged(wxSplitterEvent& event)
{
    event.Skip();
}

void SplitterWindow::OnPositionChanging(wxSplitterEvent& event)
{
    event.Skip();
}

void SplitterWindow::OnDClick(wxSplitterEvent& event)
{
    event.Skip();
}

void SplitterWindow::OnUnsplitEvent(wxSplitterEvent& event)
{
    event.Skip();
}

void SplitterWindow::splitVertically()
{
    SplitVertically(left_, right_, 100);
}

void SplitterWindow::onSelectRoot()
{
    left_->onSelectRoot();
}

void SplitterWindow::update()
{
    left_->update();
    right_->update();
}

void SplitterWindow::setSize(int x, int y, int width, int height, int sizeFlags)
{
    SetSize(x, y, width, height, sizeFlags);
    left_->SetSize(x, y, width, height);
    right_->SetSize(x, y, width, height);
}

std::wstring SplitterWindow::selectedSourcePath() const
{
    return left_->selectedPath();
}

std::wstring SplitterWindow::selectedTargetPath() const
{
    return right_->selectedPath();
}

bool SplitterWindow::mouseClicked() const
{
    bool leftClicked = left_->mouseClicked();  // [SplitterWindow.md#both-trees-polled]
    bool rightClicked = right_->mouseClicked();
    return leftClicked || rightClicked;
}