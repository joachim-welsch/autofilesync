#include "TreeItemData.h"

#include "wx/treectrl.h"

static inline const wxString Bool2String(bool b)
{
    return b ? "" : "not ";
}

void TreeItemData::ShowInfo(wxTreeCtrl* tree)
{
    wxLogMessage("Item '%s': %sselected, %sexpanded, %sbold,\n"
        "%u children (%u immediately under this item).",
        description_,
        Bool2String(tree->IsSelected(GetId())),
        Bool2String(tree->IsExpanded(GetId())),
        Bool2String(tree->IsBold(GetId())),
        unsigned(tree->GetChildrenCount(GetId())),
        unsigned(tree->GetChildrenCount(GetId(), false)));
}
