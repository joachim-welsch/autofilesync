# PhoneDirDialog

Module comment for PhoneDirDialog.h / PhoneDirDialog.cpp.

## PhoneDirDialog

Chooses a folder on a paired phone for the source or target of a synchronisation: the phone
from a list, the folder from the usual Android folders or typed in. No live browsing in this
version: the phone's file server is open only during a pass (DeviceLink.md#serve), so a
directory dialog would mostly say "open the app first". The common folders cover what a user
sets up in practice, and a typed path is checked by the first pass like any other.

## folders

The folders Android itself creates in the shared storage, which is the server's root: the
camera's DCIM/Camera first, because backing up photos is what most users come for.

## device_of_path

A path of the form "<device id>:<folder>" names a paired phone; the id is a valid SSH account
name on purpose (Config.md#device-accounts), so the rest of the program handles the path as an
SSH path.

## display_path

The same path with the phone's name in place of its 32-digit id and a phone symbol, for the
places that show a path to the user; the configuration and the sync classes keep the id.

## current-path

Editing an existing phone path preselects its phone and folder, like the SSH dialog keeps the
remote path that is already configured.
