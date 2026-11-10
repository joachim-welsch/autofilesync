# SecretBlob

Module comment for SecretBlob.h / SecretBlob.cpp.

## secretblob

Wraps the content of a key file - the password key (PasswordCrypto.md) and the private key of
the device identity (DeviceIdentity.md) - so that the file alone is worthless. Both files lie
beside the configuration they protect; a backup, a cloud folder or a support archive that takes
the directory along would otherwise carry the key with the lock.

On Windows the content is sealed with the Data Protection API under the logged-in user's
credentials: only that user, on that PC, gets it back. The background service runs under the
same account (see the installer), so the program and the service read the same files. On the
other platforms the content stays as it is; there the file mode 0600 is the protection, set by
the callers.

The price on Windows: the files no longer travel. A configuration directory copied to another
PC, or one restored after the Windows password was reset by an administrator rather than
changed by the user, yields keys that cannot be opened; the stored SSH and mail passwords then
have to be typed in again and the phones paired again.

## marker

Tells a sealed file from one written before sealing existed. A raw password key consists of 32
random bytes and a private key of PEM text, so neither starts with it by accident.

## protect

Empty result when the system refuses to seal - for a caller that is the same as a file that
cannot be written: nothing unprotected is stored in its place.

## unprotect

Content without the marker is handed back unchanged: it was written by a version before
sealing, or on a platform without it. An empty result means the seal belongs to another user
or PC, or the file is damaged.

## outdated

True for content that unprotect() accepted but that should be sealed now; the caller writes it
again. Always false where nothing is sealed.
