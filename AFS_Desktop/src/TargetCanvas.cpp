#include "TargetCanvas.h"
#include "TreeCtrl.h"
#include "config.h"

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
        treeCtrl_->addBranch(i.target(), tree_, i.syncItemType());
    }
}

