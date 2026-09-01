#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Canvas.h"
#include "config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using util::tul;

namespace guitest {
namespace {

const wxString syncItemName = "guitest-baumansicht";

// Mirrors Canvas::ShowContextMenu's own item set and how wxWindow::PopupMenu()
// primes it (SetInvokingWindow(), then UpdateUI()) - the actual native popup
// cannot be driven headlessly (see UserActions.cpp's activateMenuItem()), so
// this exercises the same wxUpdateUIEvent path without opening one.
bool editAndDeleteEnabledFor(wxWindow& invokingCanvas)
{
    wxMenu menu;
    menu.Append(tul(id::EditSync), "&Editiere");
    menu.Append(tul(id::DeleteSync), L"&Lösche");
    menu.SetInvokingWindow(&invokingCanvas);
    menu.UpdateUI();
    menu.SetInvokingWindow(nullptr);
    return menu.IsEnabled(tul(id::EditSync)) && menu.IsEnabled(tul(id::DeleteSync));
}

}

TEST_F(GuiTest, KontextmenueInBaumansichtAktivWennSyncItemAusgewaehlt)
{
    const SyncItemInput input{syncItemName,
        GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));
    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();
    ASSERT_TRUE(Config::instance().findByName(syncItemName.ToStdWstring()).has_value())
        << "Die Synchronisation wurde nicht angelegt.";

    activateMenuItem(mainWindow(), tul(id::View_Tree));
    pump();

    wxWindow* sourceCanvas = wxWindow::FindWindowByName("source", &mainWindow());
    ASSERT_NE(sourceCanvas, nullptr) << "Das Quell-Panel der Baumansicht wurde nicht gefunden.";
    wxTreeCtrl* tree = dynamic_cast<wxTreeCtrl*>(find(*sourceCanvas, ID_SRC_TREE_CTRL));
    ASSERT_NE(tree, nullptr) << "Der Baum des Quell-Panels wurde nicht gefunden.";

    // Nothing belonging to the SyncItem selected yet: still grayed out.
    tree->SelectItem(tree->GetRootItem());
    pump();
    EXPECT_FALSE(editAndDeleteEnabledFor(*sourceCanvas))
        << "Das Kontextmenue sollte ohne Auswahl eines SyncItem deaktiviert sein.";

    const wxTreeItemId sourceItem = findTreeItemForPath(*tree, GuiTestSandbox::sourceDir());
    ASSERT_TRUE(sourceItem.IsOk()) << "Der Quellordner der Synchronisation fehlt im Baum.";
    tree->SelectItem(sourceItem);
    pump();
    EXPECT_TRUE(editAndDeleteEnabledFor(*sourceCanvas))
        << "Das Kontextmenue sollte bedienbar sein, wenn der Quellordner des SyncItem ausgewaehlt ist.";
}

}
