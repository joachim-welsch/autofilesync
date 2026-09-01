#include "SplitterWindow.h"

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/dcmirror.h"
#endif

#include "util.h"
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
    //wxLogStatus(m_frame, "Position has changed, now = %d (or %d)",
    //            event.GetSashPosition(), GetSashPosition());
    event.Skip();
}

void SplitterWindow::OnPositionChanging(wxSplitterEvent& event)
{
    //wxLogStatus(m_frame, "Position is changing, now = %d (or %d)",
    //            event.GetSashPosition(), GetSashPosition());
    event.Skip();
}

void SplitterWindow::OnDClick(wxSplitterEvent& event)
{
    //parent_->SetStatusText("Splitter double clicked", 1);
    event.Skip();
}

void SplitterWindow::OnUnsplitEvent(wxSplitterEvent& event)
{
    //parent_->SetStatusText("Splitter unsplit", 1);
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
    // Both calls must always happen: TreeCtrl::mouseClicked() only resets its
    // latch once it has been polled from "the 2 instances" it expects (a
    // static counter shared across all TreeCtrl objects, reset at count==2).
    // "||" short-circuits and skips the second call whenever the first
    // returns true, so that reset never fired for the left tree - its latch
    // stayed true forever after one click, once left_ != right_ in click state.
    bool leftClicked = left_->mouseClicked();
    bool rightClicked = right_->mouseClicked();
    return leftClicked || rightClicked;
}