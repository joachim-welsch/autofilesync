#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

// Only characters wxUIActionSimulator can produce on a plain keyboard layout.
const wxString syncItemName = "guitest-sicherung";

}

TEST_F(GuiTest, AngelegteSynchronisationIstInKonfigurationUndListeVorhanden)
{
    const SyncItemInput input{syncItemName,
        GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));

    activateMenuItem(mainWindow(), tul(id::File_new_Synchronization));
    pump();

    const auto created = Config::instance().findByName(syncItemName.ToStdWstring());
    ASSERT_TRUE(created.has_value()) << "Die Synchronisation wurde nicht angelegt.";
    EXPECT_EQ(created->source(), input.source.ToStdWstring());
    EXPECT_EQ(created->target(), input.target.ToStdWstring());
    EXPECT_TRUE(listShows(syncItemName.ToStdWstring())) << "Die Synchronisation fehlt in der Liste.";
}

}
