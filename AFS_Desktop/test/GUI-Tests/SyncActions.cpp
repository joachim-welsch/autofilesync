#include "SyncActions.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "Config.h"
#include "MainWindow.h"
#include "ProgressDialog.h"
#include "Synchronize.h"
#include "TextResources.h"
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

bool addSyncItem(MainWindow& window, const SyncItemInput& input)
{
    ModalDialogWatcher::instance().expect(txt::AddSynchronisation, fillSyncItemDialog(input));
    activateMenuItem(window, tul(id::File_new_Synchronization));
    pump();
    return Config::instance().find_by_name(input.name.ToStdWstring()).has_value();
}

bool waitUntilSyncFinished(MainWindow& window, int timeoutMilliseconds)
{
    return waitUntil([&window] {  // [SyncActions.md#waitUntilSyncFinished]
        return window.syncInterface().actSyncType() == SyncType::None && !Synchronize::has_instance();
    }, timeoutMilliseconds);
}

bool syncOnce(MainWindow& window, int timeoutMilliseconds)
{
    activateMenuItem(window, tul(id::SyncOnce));
    pump();
    return waitUntilSyncFinished(window, timeoutMilliseconds);
}

bool syncMenuItemEnabled(MainWindow& window, int menuItemId)
{
    return window.menu().syncMenu()->IsEnabled(menuItemId);
}

ProgressDialog* progressDialog()
{
    for (wxWindow* window : wxTopLevelWindows) {
        if (auto* dialog = dynamic_cast<ProgressDialog*>(window); dialog != nullptr && dialog->IsShown())
            return dialog;
    }
    return nullptr;
}

}
