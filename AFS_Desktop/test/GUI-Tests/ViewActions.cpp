#include "ViewActions.h"
#include "EventPump.h"
#include "UserActions.h"
#include "Canvas.h"
#include "GridCtrl.h"
#include "MainWindow.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using utilwx::tul;

namespace guitest {
namespace {

wxTreeCtrl* treeOf(MainWindow& window, const char* canvasName)
{
    wxWindow* canvas = wxWindow::FindWindowByName(canvasName, &window);
    return canvas != nullptr ? dynamic_cast<wxTreeCtrl*>(find(*canvas, ID_SRC_TREE_CTRL)) : nullptr;
}

}

void showView(MainWindow& window, id view)
{
    activateMenuItem(window, tul(view));
    pump();
}

GridCtrl* gridView(MainWindow& window)
{
    return dynamic_cast<GridCtrl*>(find(window, tul(id::GridCtrl)));
}

wxTreeCtrl* sourceTree(MainWindow& window)
{
    return treeOf(window, "source");
}

wxTreeCtrl* targetTree(MainWindow& window)
{
    return treeOf(window, "target");
}

ListViewRestored::~ListViewRestored()
{
    showView(window_, id::View_List);
}

}
