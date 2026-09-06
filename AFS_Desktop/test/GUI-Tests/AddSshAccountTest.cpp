#include "GuiTest.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "SshAccountDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

// The reported defect: a password containing "$+!" was believed to be rejected
// by the dialog. Covered here with every special character reachable on a
// keyboard, since that is what the dialog has to accept.
const wxString passwordWithSpecialCharacters = R"(geheim$+!"#%&/()=?@[]{}\<>|,.;:-_*'^~`)";
const wxString accountName = "guitest-ssh";

}

TEST_F(GuiTest, SshPasswortMitSonderzeichenWirdUebernommen)
{
    const SshAccountInput input{accountName, "localhost", "pi", passwordWithSpecialCharacters};
    ModalDialogWatcher::instance().expect(txt::SshAccounts, addSshAccountAndClose());
    ModalDialogWatcher::instance().expect(txt::AddSshConnection, fillSshAccountDialog(input));

    activateMenuItem(mainWindow(), tul(id::EditSshAccounts));
    pump();

    const auto created = Config::instance().findSshAccountByName(accountName.ToStdString());
    ASSERT_TRUE(created.has_value()) << "Die SSH-Verbindung wurde nicht angelegt.";
    EXPECT_EQ(created->password(), std::string(passwordWithSpecialCharacters.ToUTF8()));
}

}
