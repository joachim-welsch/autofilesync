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
#include "AutoSave.h"
#include "Config.h"
#include "Translation.h"
#include <wx/msgdlg.h>

using namespace AFS_Core;

namespace autosave {

bool save(wxWindow* parent)
{
    Config& cfg = Config::instance();
    if (cfg.write(cfg.pathname()))
        return true;
    wxMessageBox(translation::translate("config_save_failed"), translation::translate("error"), wxOK | wxICON_ERROR, parent);
    return false;
}

bool after_change(wxWindow* parent)
{
    return !Config::instance().changed() || save(parent);
}

}
