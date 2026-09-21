#pragma once

#include <wx/defs.h>
#include "id.h"

class MainWindow;
class GridCtrl;
class wxTreeCtrl;

namespace guitest {  // [ViewActions.md#ViewActions]

void showView(MainWindow& window, id view);
GridCtrl* gridView(MainWindow& window);
wxTreeCtrl* sourceTree(MainWindow& window);
wxTreeCtrl* targetTree(MainWindow& window);

class ListViewRestored {  // [ViewActions.md#ListViewRestored]
public:
    explicit ListViewRestored(MainWindow& window): window_(window) {}
    ~ListViewRestored();
private:
    MainWindow& window_;
};

}
