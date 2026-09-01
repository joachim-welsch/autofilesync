#include "SourceCanvas.h"
#include "TreeCtrl.h"
#include "config.h"

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
        treeCtrl_->addBranch(i.source(), tree_, i.syncItemType());
    }
    treeCtrl_->ExpandNode(treeCtrl_->rootId());
}
