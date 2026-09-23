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
#include "ModalDialogWatcher.h"
#include "EventPump.h"
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

namespace guitest {

ModalDialogWatcher& ModalDialogWatcher::instance()
{
    static ModalDialogWatcher* watcher = [] {  // [ModalDialogWatcher.md#never-destroyed]
        auto* created = new ModalDialogWatcher;
        created->Register();
        return created;
    }();
    return *watcher;
}

void ModalDialogWatcher::expect(const wxString& title, DialogAction action)
{
    expected_.push_back({title, std::move(action)});
}

void ModalDialogWatcher::answer(const wxString& title, int buttonId)
{
    expected_.push_back({title, DialogAction(), buttonId});
}

void ModalDialogWatcher::choosePath(const wxString& title, const wxString& path)
{
    expected_.push_back({title, DialogAction(), wxID_OK, path});
}

namespace {

wxString nameOf(wxDialog& dialog)  // [ModalDialogWatcher.md#nameOf]
{
    if (auto* fileDialog = dynamic_cast<wxFileDialog*>(&dialog))
        return fileDialog->GetMessage();
    if (auto* dirDialog = dynamic_cast<wxDirDialog*>(&dialog))
        return dirDialog->GetMessage();
    return dialog.GetTitle();
}

bool setChosenPath(wxDialog& dialog, const wxString& path)  // [ModalDialogWatcher.md#choosePath]
{
    if (auto* fileDialog = dynamic_cast<wxFileDialog*>(&dialog)) {
        fileDialog->SetPath(path);
#ifdef __WXGTK__
        waitUntil([fileDialog] { return !fileDialog->GetPath().empty(); });  // [ModalDialogWatcher.md#gtk-chooser-is-asynchronous]
#endif
        return true;
    }
    if (auto* dirDialog = dynamic_cast<wxDirDialog*>(&dialog)) {
#ifdef __WXGTK__
        dirDialog->wxDirDialogBase::SetPath(path);  // [ModalDialogWatcher.md#gtk-chooser-is-asynchronous]
#else
        dirDialog->SetPath(path);
#endif
        return true;
    }
    return false;
}

}

void ModalDialogWatcher::reset()
{
    expected_.clear();
    unexpected_.clear();
    answered_.clear();
}

int ModalDialogWatcher::Enter(wxDialog* dialog)
{
    const wxString name = nameOf(*dialog);
    if (expected_.empty() || expected_.front().title != name) {
        unexpected_.push_back(name);
        return wxID_CANCEL;
    }
    const Expectation expectation = expected_.front();
    expected_.pop_front();
    if (!expectation.path.empty()) {
        if (!setChosenPath(*dialog, expectation.path))
            unexpected_.push_back(name + " (kein Datei- oder Verzeichnisdialog)");
        return expectation.answer;
    }
    if (expectation.answer != wxID_NONE) {  // [ModalDialogWatcher.md#answer]
        auto* messageDialog = dynamic_cast<wxMessageDialog*>(dialog);
        answered_.push_back(messageDialog != nullptr ? messageDialog->GetMessage() : name);
        return expectation.answer;
    }
    dialog->CallAfter([dialog, action = expectation.action] { action(*dialog); });  // [ModalDialogWatcher.md#queued-not-called]
    return wxID_NONE;
}

}
