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
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString NO_FILES_FOUND = "Es wurden keine zu synchronisierenden Dateien gefunden.";

SyncItemInput dirSync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

}

TEST_F(GuiTest, EinmalSynchronisierenKopiertDateiInsZiel)  // [SyncOnceTest.md#EinmalSynchronisierenKopiertDateiInsZiel]
{
    const SandboxTree tree(GuiTestSandbox::root() / "einmal", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-einmal"))) << "Die Synchronisation wurde nicht angelegt.";

    ASSERT_TRUE(syncOnce(mainWindow())) << "Die Synchronisation ist nicht zu Ende gelaufen.";

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht") << "Die Datei wurde nicht ins Ziel kopiert.";
    EXPECT_EQ(tree.entries(), (std::vector<std::string>{"quelle", "quelle/bericht.txt", "ziel", "ziel/bericht.txt"}));
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach dem Lauf gesperrt.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync))) << "\"Start\" bleibt nach dem Lauf gesperrt.";
}

TEST_F(GuiTest, EinmalSynchronisierenVerzeichnisbaumKopiertAlleEbenen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "baum",
        {{"quelle/oben.txt", "1"}, {"quelle/a/mitte.txt", "2"}, {"quelle/a/b/unten.txt", "3"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-baum");
    input.type = SyncItemType::Tree;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/oben.txt"), "1");
    EXPECT_EQ(tree.read("ziel/a/mitte.txt"), "2");
    EXPECT_EQ(tree.read("ziel/a/b/unten.txt"), "3") << "Die unterste Ebene fehlt im Ziel.";
}

TEST_F(GuiTest, EinmalSynchronisierenVerzeichnisKopiertKeineUnterverzeichnisse)  // [SyncOnceTest.md#dir-versus-tree]
{
    const SandboxTree tree(GuiTestSandbox::root() / "flach", {{"quelle/oben.txt", "1"}, {"quelle/a/unten.txt", "2"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-flach")));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/oben.txt"), "1");
    EXPECT_FALSE(tree.exists("ziel/a")) << "Typ Verzeichnis hat ein Unterverzeichnis mitkopiert.";
}

TEST_F(GuiTest, EinmalSynchronisierenOhneAenderungMeldetNichtsZuTun)  // [SyncOnceTest.md#nothing-to-do]
{
    const SandboxTree tree(GuiTestSandbox::root() / "unveraendert", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-unveraendert")));
    ASSERT_TRUE(syncOnce(mainWindow()));
    ASSERT_TRUE(tree.exists("ziel/bericht.txt"));

    ModalDialogWatcher::instance().answer("Info", wxID_OK);
    ASSERT_TRUE(syncOnce(mainWindow()));

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der zweite Lauf hat den Hinweis nicht gezeigt.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), NO_FILES_FOUND);
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach dem Hinweis gesperrt.";
}

TEST_F(GuiTest, InaktiveSynchronisationWirdUebersprungen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "inaktiv", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-inaktiv");
    input.options.enabled = false;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ModalDialogWatcher::instance().answer("Info", wxID_OK);
    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_FALSE(tree.exists("ziel/bericht.txt")) << "Eine inaktive Synchronisation hat kopiert.";
    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u);
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), NO_FILES_FOUND);
}

TEST_F(GuiTest, StartOhneSynchronisationZeigtHinweis)  // [SyncOnceTest.md#start-without-sync-item]
{
    ASSERT_TRUE(Config::instance().syncitems().empty()) << "Die Sandbox enthält schon Synchronisationen.";
    ModalDialogWatcher::instance().answer("Achtung", wxID_OK);

    activateMenuItem(mainWindow(), tul(id::StartSync));
    ASSERT_TRUE(waitUntil([] { return !ModalDialogWatcher::instance().answeredMessages().empty(); }))
        << "Der Hinweis auf die fehlende Synchronisation kam nicht.";
    activateMenuItem(mainWindow(), tul(id::StopSync));
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow()));

    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), "Keine Dateien zum Synchronisieren angegeben!");
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync)));
    EXPECT_FALSE(syncMenuItemEnabled(mainWindow(), tul(id::StopSync)));
}

}
