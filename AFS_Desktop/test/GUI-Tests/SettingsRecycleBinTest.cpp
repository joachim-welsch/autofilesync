#include "GuiTest.h"
#include "GuiTestSandbox.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "SettingsDialogDriver.h"
#include "UserActions.h"
#include "FileDelete.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <RecycleBin.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

// The recycle bin path is process wide state that GuiTest::TearDown() does not
// cover, and the deleted file has to be put back where the sandbox snapshot
// expects it - both must happen even when an assertion ends the test early.
class RestoredState {
public:
    RestoredState(): binPath_(RecycleBin::instance().binPath()) {}
    ~RestoredState() {
        RecycleBin::instance().binPath(binPath_);
    }
private:
    fs::path binPath_;
};

std::string readFile(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

}

TEST_F(GuiTest, NeuerPapierkorbPfadWirdUebernommenUndDateiDorthinVerschoben)
{
    const RestoredState restoredState;
    const fs::path binPath = GuiTestSandbox::root() / "papierkorb";
    const fs::path deletedFile = GuiTestSandbox::sourceDir() / "beispiel.txt";
    const std::string contentBefore = readFile(deletedFile);
    ASSERT_FALSE(fs::exists(binPath)) << "Der Papierkorb existiert schon vor dem Test.";

    ModalDialogWatcher::instance().expect(txt::EditSettings,
        fillSettingsDialog(binPath.wstring(), DeleteMode::RecycleBin));
    activateMenuItem(mainWindow(), tul(id::EditSettings));
    pump();

    ASSERT_EQ(RecycleBin::instance().binPath(), binPath) << "Der eingegebene Pfad wurde nicht übernommen.";
    ASSERT_EQ(Config::instance().settings().deleteMode(), DeleteMode::RecycleBin);

    ASSERT_TRUE(deleteFile(deletedFile.wstring())) << "Die Datei wurde nicht in den Papierkorb verschoben.";

    EXPECT_FALSE(fs::exists(deletedFile));
    EXPECT_TRUE(fs::exists(binPath)) << "Der Papierkorb wurde nicht angelegt.";
    const RecycleBin::ItemList items = RecycleBin::instance().listItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(std::regex_match(items[0].filename().wstring(),
        std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_beispiel\.txt)")))
        << "Der Zeitstempel fehlt im Namen im Papierkorb.";
    EXPECT_EQ(readFile(binPath / items[0]), contentBefore) << "Der Inhalt hat das Verschieben nicht überlebt.";

    // Puts the sandbox back the way GuiTest::TearDown() expects it; restoreItem()
    // cannot be used here because it restores relative to the working directory,
    // not to the absolute place the file came from.
    fs::rename(binPath / items[0], deletedFile);
    fs::remove_all(binPath);
}

}
