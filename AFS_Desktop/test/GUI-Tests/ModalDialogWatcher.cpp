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

void ModalDialogWatcher::answer(const wxString& title, int button_id)
{
    expected_.push_back({title, DialogAction(), button_id});
}

void ModalDialogWatcher::choose_path(const wxString& title, const wxString& path)
{
    expected_.push_back({title, DialogAction(), wxID_OK, path});
}

namespace {

wxString name_of(wxDialog& dialog)  // [ModalDialogWatcher.md#name_of]
{
    if (auto* file_dialog = dynamic_cast<wxFileDialog*>(&dialog))
        return file_dialog->GetMessage();
    if (auto* dir_dialog = dynamic_cast<wxDirDialog*>(&dialog))
        return dir_dialog->GetMessage();
    return dialog.GetTitle();
}

bool set_chosen_path(wxDialog& dialog, const wxString& path)  // [ModalDialogWatcher.md#choose_path]
{
    if (auto* file_dialog = dynamic_cast<wxFileDialog*>(&dialog)) {
        file_dialog->SetPath(path);
#ifdef __WXGTK__
        wait_until([file_dialog] { return !file_dialog->GetPath().empty(); });  // [ModalDialogWatcher.md#gtk-chooser-is-asynchronous]
#endif
        return true;
    }
    if (auto* dir_dialog = dynamic_cast<wxDirDialog*>(&dialog)) {
#ifdef __WXGTK__
        dir_dialog->wxDirDialogBase::SetPath(path);  // [ModalDialogWatcher.md#gtk-chooser-is-asynchronous]
#else
        dir_dialog->SetPath(path);
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
    ++entered_;
    const wxString name = name_of(*dialog);
    if (expected_.empty() || expected_.front().title != name) {
        unexpected_.push_back(name);
        return wxID_CANCEL;
    }
    const Expectation expectation = expected_.front();
    expected_.pop_front();
    if (!expectation.path.empty()) {
        if (!set_chosen_path(*dialog, expectation.path))
            unexpected_.push_back(name + " (kein Datei- oder Verzeichnisdialog)");
        return expectation.answer;
    }
    if (expectation.answer != wxID_NONE) {  // [ModalDialogWatcher.md#answer]
        auto* message_dialog = dynamic_cast<wxMessageDialog*>(dialog);
        answered_.push_back(message_dialog != nullptr ? message_dialog->GetMessage() : name);
        return expectation.answer;
    }
    dialog->CallAfter([dialog, action = expectation.action] { action(*dialog); });  // [ModalDialogWatcher.md#queued-not-called]
    return wxID_NONE;
}

}
