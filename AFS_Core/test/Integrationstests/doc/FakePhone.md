# FakePhone

Module comment for FakePhone.h / FakePhone.cpp.

## FakePhone

The phone's side of the control channel (DeviceLink.md#protocol), as far as a test needs it:
a TLS client with its own certificate that sends one line and collects the reply lines until
the server closes. The real app does the same in Kotlin; keeping this one in C++ lets the
link's tests run without a device and lets the GUI tests pair a "phone" through the dialog.

## identity

The phone's certificate is made by DeviceIdentity::load_or_create in a directory of the test's
own, which is the same self-signed shape the Android keystore produces, so the fingerprint
handling on the PC side is exercised with a certificate it did not issue.

## send

Pins nothing (SSL_VERIFY_NONE) but records the server's fingerprint, so a test asserts the
pinning the real app does: the certificate the server presents is the one the QR code names.
A send without certificate is the client the link must refuse.

## type

The reply lines are JSON; type() and field() read one of them with boost, the same parser
the link uses for requests, with "" for a missing field or a malformed line.

## SftpOffer

What a hello says about the phone's SFTP server; with serving true the link runs a pass
(DeviceLink.md#serve). The tests point it at the local sshd (port 22, the SSH test account), so
the "phone" is this machine and the files the pass copies are ordinary local files.
