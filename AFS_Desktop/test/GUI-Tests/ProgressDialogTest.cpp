#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "ProgressDialog.h"
#include "id.h"
#include "utilwx.h"
#include <wx/stattext.h>

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

constexpr int MANY_FILES = 1500;  // [ProgressDialogTest.md#MANY_FILES]

FileContents manyFiles()
{
    FileContents files{{"ziel/", ""}};
    for (int i = 0; i < MANY_FILES; ++i)
        files.push_back({"quelle/datei" + std::to_string(i) + ".txt", std::string(4096, 'x')});
    return files;
}

SyncItemInput dirSync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

wxString currentFileShown(ProgressDialog& dialog)
{
    for (wxWindow* child : dialog.GetChildren()) {
        if (auto* text = dynamic_cast<wxStaticText*>(child))
            return text->GetLabelText();
    }
    return wxString();
}

std::size_t copiedFiles(const SandboxTree& tree)
{
    std::size_t count = 0;
    for (const std::string& entry : tree.entries())
        count += entry.starts_with("ziel/") ? 1 : 0;
    return count;
}

}

TEST_F(GuiTest, FortschrittsdialogZeigtAktuelleDateiUndSchliesstSich)  // [ProgressDialogTest.md#FortschrittsdialogZeigtAktuelleDateiUndSchliesstSich]
{
    const SandboxTree tree(GuiTestSandbox::root() / "fortschritt", manyFiles());
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-fortschritt")));

    activateMenuItem(mainWindow(), tul(id::SyncOnce));
    ASSERT_TRUE(waitUntil([] { return progressDialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ProgressDialog& dialog = *progressDialog();
    EXPECT_EQ(dialog.GetTitle(), "Fortschritt");
    EXPECT_TRUE(waitUntil([&dialog] { return currentFileShown(dialog).Contains("datei"); }))
        << "Der Dialog zeigt keine Datei an, zuletzt: " << currentFileShown(dialog);
    EXPECT_TRUE(find(dialog, wxID_CANCEL)->IsEnabled()) << "Abbrechen ist während des Laufs gesperrt.";

    ASSERT_TRUE(waitUntilSyncFinished(mainWindow()));
    EXPECT_TRUE(waitUntil([] { return progressDialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach dem Lauf offen.";
    EXPECT_EQ(copiedFiles(tree), MANY_FILES);
}

TEST_F(GuiTest, AbbrechenImFortschrittsdialogBeendetDenLauf)  // [ProgressDialogTest.md#AbbrechenImFortschrittsdialogBeendetDenLauf]
{
    const SandboxTree tree(GuiTestSandbox::root() / "abbruch", manyFiles());
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-abbruch")));

    activateMenuItem(mainWindow(), tul(id::SyncOnce));
    ASSERT_TRUE(waitUntil([] { return progressDialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ASSERT_TRUE(clickOn(find(*progressDialog(), wxID_CANCEL))) << "Abbrechen ließ sich nicht klicken.";
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow())) << "Nach Abbrechen kam der Lauf nicht zur Ruhe.";

    const std::size_t copied = copiedFiles(tree);
    EXPECT_LT(copied, MANY_FILES) << "Abbrechen hat den Lauf nicht unterbrochen.";
    EXPECT_TRUE(waitUntil([] { return progressDialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach Abbrechen offen.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce))) << "Nach Abbrechen bleibt \"Einmal synchronisieren\" gesperrt.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync)));
}

}
