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
	virtual bool isLeaf() const = 0;
};

using PTreeItem = std::shared_ptr<TreeItem>;

class TreeNode: public TreeItem {
	using SubNodes = std::vector<std::shared_ptr<TreeItem>>;
	SubNodes     subNodes_;
public:
	using TreeItem::TreeItem;
	void add(std::shared_ptr<TreeItem> item) override;
	bool isLeaf() const override { return false;  }
	friend class Tree;
};

class Leaf: public TreeItem {
	wxTreeItemId reference_; // Source or target
public:
	Leaf(wxTreeItemId id, wxTreeItemId reference);
	bool isLeaf() const override { return true; }
};

class Tree: public TreeNode {
	using IdMap = std::map<wxTreeItemId, std::shared_ptr<TreeItem>>;
	using PathMap = std::multimap<std::wstring, std::shared_ptr<TreeItem>>;
	using IdPathMap = std::map<wxTreeItemId, std::wstring>;
	IdMap	idMap_;
	PathMap pathMap_;
	IdPathMap idPathMap_;
public:
	Tree(): TreeNode(nullptr) {}
	void addNode(wxTreeItemId parent, wxTreeItemId id, const std::wstring& path);
	void addLeaf(wxTreeItemId parent, wxTreeItemId id, const std::wstring& path, wxTreeItemId reference);
	std::optional<PTreeItem> find(wxTreeItemId id) const;
	std::wstring path(wxTreeItemId id) const;
	std::optional<wxTreeItemId> id(const std::wstring& path) const;
	std::vector<wxTreeItemId> ids(const std::wstring& path) const;
	void clear();
};
