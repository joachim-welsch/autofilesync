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
#include "Tree.h"

using std::make_pair;
using std::nullopt;
using std::optional;

TreeItem::~TreeItem() {}

Leaf::Leaf(wxTreeItemId id, wxTreeItemId reference)
	: TreeItem(id)
	, reference_(reference)
{}

void TreeNode::add(std::shared_ptr<TreeItem> item)
{
	subNodes_.push_back(item);
}

void Tree::addLeaf(wxTreeItemId parentId, wxTreeItemId id, const std::wstring& path, wxTreeItemId ref)
{
	auto leaf = std::make_shared<Leaf>(id, ref);
	auto found = idMap_.find(parentId);
	std::shared_ptr<TreeItem> base;
	if (found == idMap_.end()) {
		add(leaf);
	} else {
		found->second->add(leaf);
	}
	pathMap_.insert(make_pair(path, leaf));
	idMap_.insert(make_pair(id, leaf));
	idPathMap_.insert(make_pair(id, path));
}

void Tree::addNode(wxTreeItemId parentId, wxTreeItemId id, const std::wstring& path)
{
	auto node = std::make_shared<TreeNode>(id);
	auto found = idMap_.find(parentId);
	std::shared_ptr<TreeItem> base;
	if (found == idMap_.end()) {
		add(node);
	}
	else {
		found->second->add(node);
	}
	pathMap_.insert(make_pair(path, node));
	idMap_.insert(make_pair(id, node));
	idPathMap_.insert(make_pair(id, path));
}

std::optional<PTreeItem> Tree::find(wxTreeItemId id) const
{
	auto found = idMap_.find(id);
	if (found == idMap_.end())
		return nullopt;
	return found->second;
}

std::wstring Tree::path(wxTreeItemId id) const
{
	const auto found = idPathMap_.find(id);
	if (found == idPathMap_.end())
		return L"";
	return found->second;
}

std::optional<wxTreeItemId> Tree::id(const std::wstring& path) const
{
	std::vector<wxTreeItemId> retval;
	auto ir = pathMap_.equal_range(path);
	for (auto itr = ir.first; itr != ir.second; ++itr)
		retval.push_back(itr->second->id());
	if (retval.size() == 1)
		return *retval.begin();
	return nullopt;
}

std::vector<wxTreeItemId> Tree::ids(const std::wstring& path) const
{
	std::vector<wxTreeItemId> retval;
	auto ir = pathMap_.equal_range(path);
	for (auto itr = ir.first; itr != ir.second; ++itr)
		retval.push_back(itr->second->id());
	return retval;
}

void Tree::clear()
{
	idMap_.clear();
	pathMap_.clear();
	idPathMap_.clear();
	subNodes_.clear();
}