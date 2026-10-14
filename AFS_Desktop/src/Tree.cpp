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
	sub_nodes_.push_back(item);
}

void Tree::add_leaf(wxTreeItemId parent_id, wxTreeItemId id, const std::wstring& path, wxTreeItemId ref)
{
	auto leaf = std::make_shared<Leaf>(id, ref);
	auto found = id_map_.find(parent_id);
	std::shared_ptr<TreeItem> base;
	if (found == id_map_.end()) {
		add(leaf);
	} else {
		found->second->add(leaf);
	}
	path_map_.insert(make_pair(path, leaf));
	id_map_.insert(make_pair(id, leaf));
	id_path_map_.insert(make_pair(id, path));
}

void Tree::add_node(wxTreeItemId parent_id, wxTreeItemId id, const std::wstring& path)
{
	auto node = std::make_shared<TreeNode>(id);
	auto found = id_map_.find(parent_id);
	std::shared_ptr<TreeItem> base;
	if (found == id_map_.end()) {
		add(node);
	}
	else {
		found->second->add(node);
	}
	path_map_.insert(make_pair(path, node));
	id_map_.insert(make_pair(id, node));
	id_path_map_.insert(make_pair(id, path));
}

std::optional<PTreeItem> Tree::find(wxTreeItemId id) const
{
	auto found = id_map_.find(id);
	if (found == id_map_.end())
		return nullopt;
	return found->second;
}

std::wstring Tree::path(wxTreeItemId id) const
{
	const auto found = id_path_map_.find(id);
	if (found == id_path_map_.end())
		return L"";
	return found->second;
}

std::optional<wxTreeItemId> Tree::id(const std::wstring& path) const
{
	std::vector<wxTreeItemId> retval;
	auto ir = path_map_.equal_range(path);
	for (auto itr = ir.first; itr != ir.second; ++itr)
		retval.push_back(itr->second->id());
	if (retval.size() == 1)
		return *retval.begin();
	return nullopt;
}

std::vector<wxTreeItemId> Tree::ids(const std::wstring& path) const
{
	std::vector<wxTreeItemId> retval;
	auto ir = path_map_.equal_range(path);
	for (auto itr = ir.first; itr != ir.second; ++itr)
		retval.push_back(itr->second->id());
	return retval;
}

void Tree::clear()
{
	id_map_.clear();
	path_map_.clear();
	id_path_map_.clear();
	sub_nodes_.clear();
}