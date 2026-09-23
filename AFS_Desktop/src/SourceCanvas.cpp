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
#include "SourceCanvas.h"
#include "TreeCtrl.h"
#include "Config.h"

using namespace AFS_Core;

using std::wstring;

SourceCanvas::SourceCanvas(wxWindow* parent)
 : Canvas(parent, "Quelle")
{
    SetName("source");
    SetToolTip("Quell-Panel");
}

void SourceCanvas::update()
{
    tree_.clear();
	treeCtrl_->update();
    addDataItems();
}

void SourceCanvas::onSelectRoot()
{
    if (!treeCtrl_->HasFlag(wxTR_HIDE_ROOT))
        treeCtrl_->SelectItem(treeCtrl_->GetRootItem());
}

void SourceCanvas::addDataItems()
{
    Config& cfg = Config::instance();
    for (const auto& i : cfg.syncitems()) {
        treeCtrl_->addBranch(i.source(), tree_, i.sync_item_type());
    }
    treeCtrl_->ExpandNode(treeCtrl_->rootId());
}
