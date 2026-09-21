#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "RecycleBin.h"
#include <algorithm>
#include <filesystem>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace guitest {
namespace {

class EmptiedRecycleBin {  // [SyncOptionsTest.md#EmptiedRecycleBin]
public:
    EmptiedRecycleBin()
    {
        const fs::path bin = RecycleBin::instance().bin_path();
        insideSandbox_ = std::mismatch(GuiTestSandbox::root().begin(), GuiTestSandbox::root().end(), bin.begin(), bin.end()).first == GuiTestSandbox::root().end();
        EXPECT_TRUE(insideSandbox_) << "Der Papierkorb liegt außerhalb der Sandbox: " << bin;
    }
    ~EmptiedRecycleBin()
    {
        std::error_code ignored;
        if (insideSandbox_)  // [SyncOptionsTest.md#EmptiedRecycleBin]
            fs::remove_all(RecycleBin::instance().bin_path(), ignored);
    }
    std::size_t items() const { return RecycleBin::instance().list_items().size(); }
private:
    bool insideSandbox_ = false;
};

SyncItemInput dirSync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

void sameWriteTime(const SandboxTree& tree, const char* first, const char* second)  // [SyncOptionsTest.md#sameWriteTime]
{
    fs::last_write_time(tree.path(second), fs::last_write_time(tree.path(first)));
}

}

TEST_F(GuiTest, ZweiWegeSyncKopiertInBeideRichtungen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "zweiwege", {{"quelle/hin.txt", "hin"}, {"ziel/zurueck.txt", "zurück"}});
    SyncItemInput input = dirSync(tree, "guitest-zweiwege");
    input.options.twoWaySync = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/hin.txt"), "hin");
    EXPECT_EQ(tree.read("quelle/zurueck.txt"), "zurück") << "Die nur im Ziel vorhandene Datei kam nicht in die Quelle.";
}

TEST_F(GuiTest, QuelleNachKopierenLoeschenEntferntDieQuelldatei)  // [SyncOptionsTest.md#recycle-bin-tests]
{
    const EmptiedRecycleBin recycleBin;
    const SandboxTree tree(GuiTestSandbox::root() / "verschieben", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-verschieben");
    input.options.deleteSourceAfter = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht");
    EXPECT_FALSE(tree.exists("quelle/bericht.txt")) << "Die Quelldatei blieb nach dem Kopieren stehen.";
    EXPECT_EQ(recycleBin.items(), 1u) << "Die Quelldatei ging nicht in den Papierkorb.";
}

TEST_F(GuiTest, GeloeschteDateienImZielLoeschenEntferntVerwaisteDatei)  // [SyncOptionsTest.md#recycle-bin-tests]
{
    const EmptiedRecycleBin recycleBin;
    const SandboxTree tree(GuiTestSandbox::root() / "spiegel", {{"quelle/bleibt.txt", "1"}, {"ziel/verwaist.txt", "2"}});
    SyncItemInput input = dirSync(tree, "guitest-spiegel");
    input.options.deleteInTarget = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_TRUE(tree.exists("ziel/bleibt.txt"));
    EXPECT_FALSE(tree.exists("ziel/verwaist.txt")) << "Die verwaiste Zieldatei wurde nicht gelöscht.";
    EXPECT_EQ(recycleBin.items(), 1u) << "Die Zieldatei ging nicht in den Papierkorb.";
}

TEST_F(GuiTest, OhneOptionBleibtVerwaisteDateiImZiel)
{
    const SandboxTree tree(GuiTestSandbox::root() / "keinspiegel", {{"quelle/bleibt.txt", "1"}, {"ziel/verwaist.txt", "2"}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-keinspiegel")));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_TRUE(tree.exists("ziel/verwaist.txt")) << "Ohne die Option wurde im Ziel gelöscht.";
}

TEST_F(GuiTest, GenerationenprinzipLegtBakDateiAn)  // [SyncOptionsTest.md#GenerationenprinzipLegtBakDateiAn]
{
    const SandboxTree tree(GuiTestSandbox::root() / "generationen", {{"quelle/bericht.txt", "alt"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-generationen");
    input.options.fatherSonPrinciple = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    ASSERT_TRUE(syncOnce(mainWindow()));
    ASSERT_EQ(tree.read("ziel/bericht.txt"), "alt");

    pump(1100);
    tree.write("quelle/bericht.txt", "neu");
    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "neu");
    EXPECT_EQ(tree.read("ziel/bericht.txt.bak.1"), "alt") << "Der alte Stand wurde nicht als .bak.1 aufgehoben.";
}

TEST_F(GuiTest, LeereDateienWerdenNurMitOptionKopiert)
{
    const SandboxTree tree(GuiTestSandbox::root() / "leer", {{"quelle/leer.txt", ""}, {"quelle/voll.txt", "x"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-leer")));
    ASSERT_TRUE(syncOnce(mainWindow()));
    ASSERT_TRUE(tree.exists("ziel/voll.txt"));
    ASSERT_FALSE(tree.exists("ziel/leer.txt")) << "Die leere Datei wurde ohne die Option kopiert.";

    const SandboxTree withOption(GuiTestSandbox::root() / "leer-mit", {{"quelle/leer.txt", ""}, {"ziel/", ""}});
    SyncItemInput input = dirSync(withOption, "guitest-leer-mit");
    input.options.syncEmptyFiles = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_TRUE(withOption.exists("ziel/leer.txt")) << "Die leere Datei wurde trotz Option nicht kopiert.";
}

TEST_F(GuiTest, VergleichNachInhaltKopiertGeaenderteDateiGleicherGroesse)  // [SyncOptionsTest.md#sameWriteTime]
{
    const SandboxTree tree(GuiTestSandbox::root() / "inhalt", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    sameWriteTime(tree, "quelle/bericht.txt", "ziel/bericht.txt");
    SyncItemInput input = dirSync(tree, "guitest-inhalt");
    input.options.compareDate = false;
    input.options.compareContent = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "AAAA") << "Der Inhaltsunterschied wurde nicht erkannt.";
}

TEST_F(GuiTest, VergleichNachDatumUebersiehtGeaenderteDateiGleicherGroesse)
{
    const SandboxTree tree(GuiTestSandbox::root() / "datum", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    sameWriteTime(tree, "quelle/bericht.txt", "ziel/bericht.txt");
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-datum")));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "BBBB") << "Ohne Inhaltsvergleich wurde bei gleichem Datum kopiert.";
}

TEST_F(GuiTest, PlatzhalterBeschraenktAufPassendeDateien)
{
    const SandboxTree tree(GuiTestSandbox::root() / "platzhalter", {{"quelle/a.txt", "1"}, {"quelle/b.log", "2"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-platzhalter");
    input.wildcard = "*.txt";
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_TRUE(tree.exists("ziel/a.txt"));
    EXPECT_FALSE(tree.exists("ziel/b.log")) << "Eine Datei außerhalb des Platzhalters wurde kopiert.";
}

TEST_F(GuiTest, AusschluesseHaltenDateienZurueck)
{
    const SandboxTree tree(GuiTestSandbox::root() / "ausschluss",
        {{"quelle/a.txt", "1"}, {"quelle/b.log", "2"}, {"quelle/geheim.txt", "3"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-ausschluss");
    input.exclusions = "*.log\ngeheim*";
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    const auto created = Config::instance().find_by_name(input.name.ToStdWstring());
    ASSERT_TRUE(created.has_value());
    ASSERT_EQ(created->exclusions().size(), 2u) << "Die Ausschlüsse kamen nicht zeilenweise in die Konfiguration.";

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_TRUE(tree.exists("ziel/a.txt"));
    EXPECT_FALSE(tree.exists("ziel/b.log")) << "Ein per Muster ausgeschlossener Typ wurde kopiert.";
    EXPECT_FALSE(tree.exists("ziel/geheim.txt")) << "Eine per Namensmuster ausgeschlossene Datei wurde kopiert.";
}

}
