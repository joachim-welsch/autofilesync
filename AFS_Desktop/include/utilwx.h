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

#include <type_traits>
#include <wx/msgdlg.h>
#include "Translation.h"

namespace utilwx {

inline bool confirm(wxWindow* parent, const wxString& question, const wxString& caption, const wxString& action)  // [utilwx.md#confirm]
{
    wxMessageDialog dialog(parent, question, caption, wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    dialog.SetYesNoLabels(action, wxString(AFS_Core::translation::translate("button_cancel")));
    return dialog.ShowModal() == wxID_YES;
}

inline wxString field_label(const char* msgid)  // [utilwx.md#field_label]
{
    return AFS_Core::translation::translate(msgid) + AFS_Core::translation::translate("label_colon");
}

template <typename E>
constexpr auto tul(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

}