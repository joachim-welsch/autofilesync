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
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <wx/treebase.h>


class TreeItem {
	wxTreeItemId id_;
public:
	TreeItem(wxTreeItemId id): id_(id) {}
	virtual ~TreeItem();
	wxTreeItemId id() const { return id_; }
	virtual void add(std::shared_ptr<TreeItem> item) {}
	virtual bool is_leaf() const = 0;
};

using PTreeItem = std::shared_ptr<TreeItem>;

class TreeNode: public TreeItem {
	using SubNodes = std::vector<std::shared_ptr<TreeItem>>;
	SubNodes     sub_nodes_;
public:
	using TreeItem::TreeItem;
	void add(std::shared_ptr<TreeItem> item) override;
	bool is_leaf() const override { return false;  }
	friend class Tree;
};

class Leaf: public TreeItem {
	wxTreeItemId reference_; // Source or target
public:
	Leaf(wxTreeItemId id, wxTreeItemId reference);
	bool is_leaf() const override { return true; }
};

class Tree: public TreeNode {
	using IdMap = std::map<wxTreeItemId, std::shared_ptr<TreeItem>>;
	using PathMap = std::multimap<std::wstring, std::shared_ptr<TreeItem>>;
	using IdPathMap = std::map<wxTreeItemId, std::wstring>;
	IdMap	id_map_;
	PathMap path_map_;
	IdPathMap id_path_map_;
public:
	Tree(): TreeNode(nullptr) {}
	void add_node(wxTreeItemId parent, wxTreeItemId id, const std::wstring& path);
	void add_leaf(wxTreeItemId parent, wxTreeItemId id, const std::wstring& path, wxTreeItemId reference);
	std::optional<PTreeItem> find(wxTreeItemId id) const;
	std::wstring path(wxTreeItemId id) const;
	std::optional<wxTreeItemId> id(const std::wstring& path) const;
	std::vector<wxTreeItemId> ids(const std::wstring& path) const;
	void clear();
};
