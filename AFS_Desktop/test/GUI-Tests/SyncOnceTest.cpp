#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

TEST_F(GuiTest, EinmalSynchronisierenKopiertDateiInsZiel)  // [SyncOnceTest.md#EinmalSynchronisierenKopiertDateiInsZiel]
{
    const SandboxTree tree(GuiTestSandbox::root() / "einmal", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input{"guitest-einmal", tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));
    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();
    ASSERT_TRUE(Config::instance().find_by_name(input.name.ToStdWstring()).has_value()) << "Die Synchronisation wurde nicht angelegt.";

    ASSERT_TRUE(syncOnce(mainWindow())) << "Die Synchronisation ist nicht zu Ende gelaufen.";

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht") << "Die Datei wurde nicht ins Ziel kopiert.";
    EXPECT_EQ(tree.entries(), (std::vector<std::string>{"quelle", "quelle/bericht.txt", "ziel", "ziel/bericht.txt"}));
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach dem Lauf gesperrt.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync))) << "\"Start\" bleibt nach dem Lauf gesperrt.";
}

}
