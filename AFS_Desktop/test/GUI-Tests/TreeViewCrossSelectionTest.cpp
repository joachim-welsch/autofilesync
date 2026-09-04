#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Canvas.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <wx/treectrl.h>

using util::tul;

namespace guitest {
namespace {

const wxString syncItemName = "guitest-baumansicht-gegenseite";

}

// A left click on a SyncItem's own node in one tree should reveal and select
// its counterpart (source<->target) in the other tree, so both sides always
// point at the same SyncItem - see MainWindow::findSyncItem() and
// Canvas::onItemLeftClicked().
TEST_F(GuiTest, LinksclickImBaumZeigtDenPfadAufDerAnderenSeite)
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
    wxWindow* targetCanvas = wxWindow::FindWindowByName("target", &mainWindow());
    ASSERT_NE(sourceCanvas, nullptr) << "Das Quell-Panel der Baumansicht wurde nicht gefunden.";
    ASSERT_NE(targetCanvas, nullptr) << "Das Ziel-Panel der Baumansicht wurde nicht gefunden.";
    wxTreeCtrl* sourceTree = dynamic_cast<wxTreeCtrl*>(find(*sourceCanvas, ID_SRC_TREE_CTRL));
    wxTreeCtrl* targetTree = dynamic_cast<wxTreeCtrl*>(find(*targetCanvas, ID_SRC_TREE_CTRL));
    ASSERT_NE(sourceTree, nullptr) << "Der Baum des Quell-Panels wurde nicht gefunden.";
    ASSERT_NE(targetTree, nullptr) << "Der Baum des Ziel-Panels wurde nicht gefunden.";

    const wxTreeItemId sourceItem = findTreeItemForPath(*sourceTree, GuiTestSandbox::sourceDir());
    const wxTreeItemId targetItem = findTreeItemForPath(*targetTree, GuiTestSandbox::targetDir());
    ASSERT_TRUE(sourceItem.IsOk()) << "Der Quellordner der Synchronisation fehlt im Baum.";
    ASSERT_TRUE(targetItem.IsOk()) << "Der Zielordner der Synchronisation fehlt im Baum.";
    sourceTree->EnsureVisible(sourceItem);
    pump();

    ASSERT_TRUE(clickOn(*sourceTree, sourceItem)) << "Der Quellordner konnte nicht angeklickt werden.";
    EXPECT_EQ(targetTree->GetSelection(), targetItem)
        << "Der Klick auf den Quellordner haette den zugehoerigen Zielordner auswaehlen sollen.";

    // And the other way round: clicking the target side reveals the source.
    sourceTree->SelectItem(sourceTree->GetRootItem(), false);
    targetTree->EnsureVisible(targetItem);
    pump();
    ASSERT_TRUE(clickOn(*targetTree, targetItem)) << "Der Zielordner konnte nicht angeklickt werden.";
    EXPECT_EQ(sourceTree->GetSelection(), sourceItem)
        << "Der Klick auf den Zielordner haette den zugehoerigen Quellordner auswaehlen sollen.";
}

}
