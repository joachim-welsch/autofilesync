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
#include "SourceCanvas.h"
#include "TreeCtrl.h"
#include "Config.h"
#include "Translation.h"

using namespace AFS_Core;

using std::wstring;

SourceCanvas::SourceCanvas(wxWindow* parent)
 : Canvas(parent, translation::translate("source"))
{
    SetName("source");
    SetToolTip(translation::translate("source_panel"));
}

void SourceCanvas::update()
{
    tree_.clear();
	tree_ctrl_->update();
    add_data_items();
}

void SourceCanvas::on_select_root()
{
    if (!tree_ctrl_->HasFlag(wxTR_HIDE_ROOT))
        tree_ctrl_->SelectItem(tree_ctrl_->GetRootItem());
}

void SourceCanvas::add_data_items()
{
    Config& cfg = Config::instance();
    for (const auto& i : cfg.syncitems()) {
        tree_ctrl_->add_branch(i.source(), tree_, i.sync_item_type());
    }
    tree_ctrl_->expand_node(tree_ctrl_->root_id());
}
