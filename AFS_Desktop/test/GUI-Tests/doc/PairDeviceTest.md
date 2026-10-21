# PairDeviceTest

Module comment for PairDeviceTest.cpp.

## PairDeviceTest

The fixture stops the device link and removes the test phone before the base class compares
the application state: the link is a singleton that would otherwise keep listening into the
next test, and a device that paired through the dialog was autosaved, which the base class
undoes by restoring the configuration file.

## pairing

The user's path end to end: the menu opens the dialog, the dialog shows a QR code and opens a
token, a phone (the integration tests' FakePhone, FakePhone.md) presents that token over TLS,
the dialog switches to "connected" and the device is in the configuration afterwards. The
token is read from the link rather than decoded from the bitmap: decoding a QR image is the
app's job, and the content is pinned by DeviceTests.md#qr.

## no-port-without-phone

Opening and closing the dialog without a phone leaves no port open (PairDeviceDialog.md#link-lifetime).

## unpair

The list shows the phone with "not reported yet", and disconnecting asks first and names the
phone (DevicesDialog.md#unpair).

## templates

The settings of each template are checked on the item phone_template_item builds, without the
dialog: delete_in_target off for photos, two-way with conflict copies and move tracking for the
shared folder, delete_in_target on for music, warn_if_unreachable off for all
(PhoneTemplatesDialog.md#templates). The dialog itself is driven once from the phone list:
"Synchronisation einrichten…" opens it, OK with the default template creates the photo item,
which the test removes again before the base class compares the configuration.
