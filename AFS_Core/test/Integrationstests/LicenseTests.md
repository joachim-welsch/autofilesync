# LicenseTests

Module comment for LicenseTests.cpp.

## SignedLicenseFile

A licence file signed with a key pair made on the spot (License.md#sign_for_test), removed
with its directory when the test leaves its scope. The tests never touch the product key:
what is under test is the reading and the check, not a particular key.

## line-endings

The signed payload is the text with `\n` line endings; a file rewritten with `\r\n` by a
Windows editor must still verify, otherwise a licensee who opens the file to look at it loses
the licence.

## issued-license

A licence made with tools/makeLicense.sh and the product's private key, pinned here with its
signature: the one test that ties the embedded public key to the key the developer signs with.
A new key pair has to update this signature, which is the intended reminder that every issued
licence stops verifying then.

## private-use

The signature makes the file a licence, the licensee decides whom it is for; an empty one is
the private-use case (License.md#private-use) and must not be mistaken for a broken file.
