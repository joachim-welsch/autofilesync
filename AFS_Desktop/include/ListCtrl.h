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
#include "wx/listctrl.h"

class ListCtrl: public wxListCtrl {
public:
    ListCtrl(wxWindow* parent,
        const wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        long style)
        : wxListCtrl(parent, id, pos, size, style)
    {}

    void addDataItems();
    std::wstring getSelected() const;
    void update();
private:
    void OnDClick(wxMouseEvent& evt);
    wxDECLARE_EVENT_TABLE();
};

