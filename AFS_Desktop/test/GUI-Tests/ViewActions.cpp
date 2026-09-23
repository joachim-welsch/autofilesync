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
#include "ViewActions.h"
#include "EventPump.h"
#include "UserActions.h"
#include "Canvas.h"
#include "GridCtrl.h"
#include "MainWindow.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using utilwx::tul;

namespace guitest {
namespace {

wxTreeCtrl* treeOf(MainWindow& window, const char* canvasName)
{
    wxWindow* canvas = wxWindow::FindWindowByName(canvasName, &window);
    return canvas != nullptr ? dynamic_cast<wxTreeCtrl*>(find(*canvas, ID_SRC_TREE_CTRL)) : nullptr;
}

}

void showView(MainWindow& window, id view)
{
    activateMenuItem(window, tul(view));
    pump();
}

GridCtrl* gridView(MainWindow& window)
{
    return dynamic_cast<GridCtrl*>(find(window, tul(id::GridCtrl)));
}

wxTreeCtrl* sourceTree(MainWindow& window)
{
    return treeOf(window, "source");
}

wxTreeCtrl* targetTree(MainWindow& window)
{
    return treeOf(window, "target");
}

ListViewRestored::~ListViewRestored()
{
    showView(window_, id::View_List);
}

}
