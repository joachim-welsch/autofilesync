#include "GuiTest.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "ListCtrl.h"
#include "util.h"
#include "utilwx.h"
#include <wx/app.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

std::string readFile(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

// Names and sizes are enough to spot every change a synchronisation could
// cause: a copied, deleted or overwritten file changes one of them.
std::vector<std::string> entriesOf(const fs::path& directory)
{
    std::vector<std::string> entries;
    for (const auto& entry : fs::recursive_directory_iterator(directory))
        entries.push_back(entry.path().lexically_relative(directory).string()
            + (entry.is_regular_file() ? ":" + std::to_string(entry.file_size()) : ""));
    std::sort(entries.begin(), entries.end());
    return entries;
}

ApplicationState captureState()
{
    ApplicationState state;
    for (const auto& item : Config::instance().syncitems())
        state.syncItemNames.push_back(util::wstring_to_string(item.name()));
    for (const auto& account : Config::instance().sshAccountsSnapshot())
        state.sshAccountNames.push_back(account.name());
    state.configFile = readFile(GuiTestSandbox::configFile());
    state.sourceEntries = entriesOf(GuiTestSandbox::sourceDir());
    state.targetEntries = entriesOf(GuiTestSandbox::targetDir());
    return state;
}

}

void GuiTest::SetUp()
{
    ModalDialogWatcher::instance().reset();
    before_ = captureState();
}

void GuiTest::TearDown()
{
    // Re-reading the untouched file undoes every in-memory change and clears the
    // "unsaved changes" flag, which would otherwise make the application ask for
    // saving while it shuts down.
    Config::instance().read(GuiTestSandbox::configFile().wstring());
    if (ListCtrl* list = syncItemList())
        list->update();
    const ApplicationState after = captureState();
    EXPECT_EQ(before_.syncItemNames, after.syncItemNames) << "Die Konfiguration wurde verändert.";
    EXPECT_EQ(before_.sshAccountNames, after.sshAccountNames) << "Die SSH-Verbindungen wurden verändert.";
    EXPECT_EQ(before_.configFile, after.configFile) << "Die Konfigurationsdatei wurde verändert.";
    EXPECT_EQ(before_.sourceEntries, after.sourceEntries) << "Das Quellverzeichnis wurde verändert.";
    EXPECT_EQ(before_.targetEntries, after.targetEntries) << "Das Zielverzeichnis wurde verändert.";
    ModalDialogWatcher& watcher = ModalDialogWatcher::instance();
    // Both branches have to be a wxString: mixing it with a string literal makes
    // the conditional ambiguous, since either side converts to the other.
    const std::vector<wxString>& unexpected = watcher.unexpectedDialogs();
    EXPECT_TRUE(unexpected.empty())
        << "Unerwarteter Dialog: " << (unexpected.empty() ? wxString() : unexpected.front());
    EXPECT_FALSE(watcher.hasPendingExpectations()) << "Ein erwarteter Dialog wurde nie geöffnet.";
    watcher.reset();
}

wxFrame& GuiTest::mainWindow() const
{
    return *static_cast<wxFrame*>(wxTheApp->GetTopWindow());
}

ListCtrl* GuiTest::syncItemList() const
{
    return dynamic_cast<ListCtrl*>(find(mainWindow(), tul(id::ListCtrl)));
}

bool GuiTest::listShows(const std::wstring& syncItemName) const
{
    ListCtrl* list = syncItemList();
    if (list == nullptr)
        return false;
    for (long i = 0; i < list->GetItemCount(); ++i) {
        if (list->GetItemText(i).ToStdWstring() == syncItemName)
            return true;
    }
    return false;
}

}
