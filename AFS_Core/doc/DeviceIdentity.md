# DeviceIdentity

Module comment for DeviceIdentity.h / DeviceIdentity.cpp.

## DeviceIdentity

The PC's own TLS key and certificate for the connection to a phone. Self-signed on purpose: there
is no authority a phone in a home network could ask. Trust comes from the QR code instead, which
carries the certificate's fingerprint (DevicePairing.md#qr_content), so the phone pins exactly
this certificate and a man in the middle has nothing to offer.

## load_or_create

Reads device.key and device.crt from the directory of the configuration; when either is
missing or unreadable, both are generated anew. A new certificate means every paired phone
stops recognising the PC and must be paired again - which is the right outcome when the key was
lost, and the reason the two files are written next to the configuration the user already keeps.

## self-signed

EC P-256, which every Android version in scope verifies, valid for twenty years from one day
before creation: a phone whose clock runs slightly behind the PC must not reject a certificate
issued "in the future". The serial number is random so that two PCs with the same host name
never produce the same certificate.

## validity

Twenty years, not a few months: nothing renews the certificate, and expiry would silently
unpair every phone one day. The fingerprint pinning makes the lifetime irrelevant for security.

## private-files

Owner-only permissions on the key file, like an SSH private key. On Windows the private key is
sealed for the user instead (SecretBlob.md#secretblob); a key file written before sealing
existed is read as it is and written again sealed.

## unsaved-identity

When the files cannot be written the identity is still used for this run, so pairing works and
the error is visible in the log; the next start generates a new identity and the phone has to
be paired again.

## certificate_fingerprint

SHA-256 over the DER encoding, lowercase hex without separators - the same form the app
computes on its side and the QR code carries, so the three can be compared as strings.
