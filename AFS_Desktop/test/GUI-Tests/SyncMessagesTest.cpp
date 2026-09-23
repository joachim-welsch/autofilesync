/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "RestoredSettings.h"
#include "SandboxTree.h"
#include "SettingsDialogDriver.h"
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
namespace {

const wxString NO_FILES = "Info";
const wxString ERRORS_BOX = "Synchronisation: Fehler";
const wxString WARNINGS_BOX = "Synchronisation: Warnung";
const wxString INFOS_BOX = "Synchronisation: Info";

void filterMessages(MainWindow& window, bool errors, bool warnings, bool infos)  // [SyncMessagesTest.md#filterMessages]
{
    SettingsInput input;
    input.showErrors = errors;
    input.showWarnings = warnings;
    input.showInfos = infos;
    ModalDialogWatcher::instance().expect(txt::EditSettings, fillSettingsDialog(input));
    activateMenuItem(window, tul(id::EditSettings));
    pump();
}

SyncItemInput dirSync(const wxString& name, const std::filesystem::path& source, const std::filesystem::path& target)
{
    SyncItemInput input{name, source.string(), target.string()};
    input.type = SyncItemType::Dir;
    return input;
}

const std::vector<wxString>& answered()
{
    return ModalDialogWatcher::instance().answeredMessages();
}

}

TEST_F(GuiTest, FehlerDesEinzelsyncsErscheintAlsMeldung)  // [SyncMessagesTest.md#unreachable-source]
{
    const RestoredSettings restored;
    filterMessages(mainWindow(), true, false, false);
    const std::filesystem::path missing = GuiTestSandbox::root() / "gibt-es-nicht";
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync("guitest-fehler", missing, GuiTestSandbox::targetDir())));
    ModalDialogWatcher::instance().answer(NO_FILES, wxID_OK);
    ModalDialogWatcher::instance().answer(ERRORS_BOX, wxID_OK);

    ASSERT_TRUE(syncOnce(mainWindow()));

    ASSERT_EQ(answered().size(), 2u) << "Die Fehlerbox nach dem Lauf fehlt.";
    EXPECT_TRUE(answered()[1].StartsWith("Fehler: ")) << answered()[1];
    EXPECT_TRUE(answered()[1].Contains(missing.string())) << "Die Fehlerbox nennt die unerreichbare Quelle nicht: " << answered()[1];
}

TEST_F(GuiTest, MeldungsfilterAusUnterdruecktDieFehlerbox)
{
    const RestoredSettings restored;
    filterMessages(mainWindow(), false, false, false);
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync("guitest-still", GuiTestSandbox::root() / "gibt-es-nicht", GuiTestSandbox::targetDir())));
    ModalDialogWatcher::instance().answer(NO_FILES, wxID_OK);

    ASSERT_TRUE(syncOnce(mainWindow()));

    EXPECT_EQ(answered().size(), 1u) << "Trotz abgewähltem Filter erschien eine Meldungsbox.";
}

TEST_F(GuiTest, WarnungDesEinzelsyncsErscheintMitFilter)  // [SyncMessagesTest.md#levels]
{
    const RestoredSettings restored;
    filterMessages(mainWindow(), true, true, false);
    const SandboxTree tree(GuiTestSandbox::root() / "warnung", {{"quelle/leer.txt", ""}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync("guitest-warnung", tree.path("quelle"), tree.path("ziel"))));
    ModalDialogWatcher::instance().answer(WARNINGS_BOX, wxID_OK);

    ASSERT_TRUE(syncOnce(mainWindow()));

    ASSERT_EQ(answered().size(), 1u) << "Die Warnung zur leeren Quelldatei erschien nicht.";
    EXPECT_TRUE(answered().front().StartsWith("Warnung: ")) << answered().front();
    EXPECT_TRUE(answered().front().Contains("leer.txt")) << answered().front();
}

TEST_F(GuiTest, InfoDesEinzelsyncsErscheintMitFilter)  // [SyncMessagesTest.md#levels]
{
    const RestoredSettings restored;
    filterMessages(mainWindow(), true, true, true);
    const SandboxTree tree(GuiTestSandbox::root() / "info", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync("guitest-info", tree.path("quelle"), tree.path("ziel"))));
    ModalDialogWatcher::instance().answer(INFOS_BOX, wxID_OK);

    ASSERT_TRUE(syncOnce(mainWindow()));

    ASSERT_EQ(answered().size(), 1u) << "Die Infos des Laufs erschienen nicht.";
    EXPECT_TRUE(answered().front().Contains("Info: ")) << answered().front();
    EXPECT_TRUE(answered().front().Contains("bericht.txt")) << "Die kopierte Datei fehlt in der Info-Box: " << answered().front();
    EXPECT_TRUE(tree.exists("ziel/bericht.txt"));
}

TEST_F(GuiTest, FortlaufenderBetriebZeigtKeineMeldungsbox)  // [SyncMessagesTest.md#continuous-is-silent]
{
    const RestoredSettings restored;
    filterMessages(mainWindow(), true, true, true);
    const SandboxTree tree(GuiTestSandbox::root() / "fortlaufend-still", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync("guitest-fortlaufend-still", tree.path("quelle"), tree.path("ziel"))));

    activateMenuItem(mainWindow(), tul(id::StartSync));
    ASSERT_TRUE(waitUntil([&tree] { return tree.exists("ziel/bericht.txt"); }, 10000));
    activateMenuItem(mainWindow(), tul(id::StopSync));
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow()));

    EXPECT_TRUE(answered().empty());
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpectedDialogs().empty()) << "Der fortlaufende Betrieb zeigte eine Box: " << ModalDialogWatcher::instance().unexpectedDialogs().front();
}

}
