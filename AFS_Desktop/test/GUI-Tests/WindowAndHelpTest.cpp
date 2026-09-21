#include "GuiTest.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "BackgroundService.h"
#include "TaskBarIcon.h"
#include "Version.h"
#include "id.h"
#include "utilwx.h"
#include <wx/html/helpctrl.h>
#include <memory>

using utilwx::tul;

namespace guitest {
namespace {

class ShownAgain {  // [WindowAndHelpTest.md#ShownAgain]
public:
    explicit ShownAgain(MainWindow& window): window_(window) {}
    ~ShownAgain()
    {
        window_.Show();
        pump();
    }
private:
    MainWindow& window_;
};

bool activateTaskBarMenuItem(TaskBarIcon& icon, const wxString& label)  // [WindowAndHelpTest.md#activateTaskBarMenuItem]
{
    const std::unique_ptr<wxMenu> menu(icon.CreatePopupMenu());
    if (menu == nullptr)
        return false;
    for (const wxMenuItem* item : menu->GetMenuItems()) {
        if (item->GetItemLabelText() != label)
            continue;
        wxCommandEvent event(wxEVT_MENU, item->GetId());
        icon.ProcessEvent(event);
        pump();
        return true;
    }
    return false;
}

wxHtmlHelpFrame* helpFrame(MainWindow& window)
{
    return dynamic_cast<wxHtmlHelpController&>(window.GetHelpController()).GetFrame();
}

class ClosedHelp {  // [WindowAndHelpTest.md#ClosedHelp]
public:
    explicit ClosedHelp(MainWindow& window): window_(window) {}
    ~ClosedHelp()
    {
        if (wxHtmlHelpFrame* frame = helpFrame(window_))
            frame->Close(true);
        pump();
    }
private:
    MainWindow& window_;
};

}

TEST_F(GuiTest, AnsichtHintergrundVerstecktFensterUndTaskleisteZeigtEsWieder)
{
    const ShownAgain shownAgain(mainWindow());
    ASSERT_TRUE(mainWindow().IsShown());
    ASSERT_NE(mainWindow().taskBarIcon(), nullptr) << "Es gibt kein Symbol im Infobereich.";

    activateMenuItem(mainWindow(), tul(id::View_Background));
    pump();
    EXPECT_FALSE(mainWindow().IsShown()) << "\"Ansicht → Hintergrund\" hat das Fenster nicht versteckt.";

    ASSERT_TRUE(activateTaskBarMenuItem(*mainWindow().taskBarIcon(), "Hauptfenster wieder herstellen"))
        << "Das Menü des Infobereichs hat keinen Eintrag zum Wiederherstellen.";
    EXPECT_TRUE(mainWindow().IsShown()) << "Das Fenster kam über das Menü des Infobereichs nicht zurück.";
}

TEST_F(GuiTest, TaskbarDoppelklickZeigtFenster)
{
    const ShownAgain shownAgain(mainWindow());
    TaskBarIcon* icon = mainWindow().taskBarIcon();
    ASSERT_NE(icon, nullptr);
    mainWindow().Show(false);
    pump();
    ASSERT_FALSE(mainWindow().IsShown());

    wxTaskBarIconEvent doubleClick(wxEVT_TASKBAR_LEFT_DCLICK, icon);
    icon->ProcessEvent(doubleClick);
    pump();

    EXPECT_TRUE(mainWindow().IsShown()) << "Der Doppelklick auf das Symbol im Infobereich zeigt das Fenster nicht.";
}

TEST_F(GuiTest, InfoDialogZeigtVersion)
{
    ModalDialogWatcher::instance().answer(L"Über AutoFileSync", wxID_OK);

    activateMenuItem(mainWindow(), tul(id::About));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der Info-Dialog erschien nicht.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().front().Contains(afsVERSION_STRING))
        << "Der Info-Dialog nennt die Version nicht: " << ModalDialogWatcher::instance().answeredMessages().front();
}

TEST_F(GuiTest, HilfeOeffnetHilfebuch)  // [WindowAndHelpTest.md#help]
{
    const ClosedHelp closedHelp(mainWindow());
    ASSERT_TRUE(helpFrame(mainWindow()) == nullptr || !helpFrame(mainWindow())->IsShown());

    activateMenuItem(mainWindow(), tul(id::Help));
    ASSERT_TRUE(waitUntil([this] { return helpFrame(mainWindow()) != nullptr && helpFrame(mainWindow())->IsShown(); }))
        << "Das Hilfefenster erschien nicht.";
    EXPECT_TRUE(helpFrame(mainWindow())->GetTitle().Contains("AutoFileSync") || !helpFrame(mainWindow())->GetTitle().empty());
}

TEST_F(GuiTest, WerkzeugleisteHilfeOeffnetHilfebuch)  // [WindowAndHelpTest.md#help]
{
    const ClosedHelp closedHelp(mainWindow());

    ASSERT_TRUE(activateToolbarTool(mainWindow(), tul(id::Help))) << "Die Werkzeugleiste hat keinen Hilfe-Knopf.";

    EXPECT_TRUE(waitUntil([this] { return helpFrame(mainWindow()) != nullptr && helpFrame(mainWindow())->IsShown(); }))
        << "Der Hilfe-Knopf der Werkzeugleiste öffnete die Hilfe nicht.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().empty()) << "Der Hilfe-Knopf zeigte einen Dialog statt der Hilfe.";
}

TEST_F(GuiTest, HintergrunddienstStartOhneDienstZeigtFehlerbox)  // [WindowAndHelpTest.md#background-service]
{
    if (BackgroundService::state() != BackgroundService::State::NotInstalled)
        GTEST_SKIP() << "Der Hintergrunddienst ist installiert; Start und Stopp brauchen Administratorrechte.";
    ModalDialogWatcher::instance().answer("Hintergrunddienst", wxID_OK);

    activateMenuItem(mainWindow(), tul(id::BackgroundServiceStart));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der fehlende Dienst blieb ohne Meldung.";
    EXPECT_FALSE(ModalDialogWatcher::instance().answeredMessages().front().empty());
}

}
