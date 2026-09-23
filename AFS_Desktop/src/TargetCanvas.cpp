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

