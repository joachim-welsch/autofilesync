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
#include "TargetCanvas.h"
#include "TreeCtrl.h"
#include "Config.h"

using namespace AFS_Core;

using std::wstring;

TargetCanvas::TargetCanvas(wxWindow* parent)
 : Canvas(parent, "Ziel")
{
	SetName("target");
	SetToolTip("Ziel-Panel");
}

void TargetCanvas::update()
{
    tree_.clear();
    treeCtrl_->update();
    addDataItems();
}

void TargetCanvas::onSelectRoot()
{}

void TargetCanvas::addDataItems()
{
    wxTreeItemId parentId = treeCtrl_->rootId();
    Config& cfg = Config::instance();
    for (const auto& i : cfg.syncitems()) {
        treeCtrl_->addBranch(i.target(), tree_, i.sync_item_type());
    }
}

