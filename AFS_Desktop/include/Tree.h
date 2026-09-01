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
