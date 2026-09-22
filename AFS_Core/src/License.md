# License

Module comment for License.h / License.cpp / LicensePublicKey.h.

## license

Reads and checks the licence file that names the licensee. The check is an Ed25519 signature
over the text of the file, made with a private key that never leaves the developer; the program
carries only the public key. Without a file the program runs as a private-use installation -
the licence changes what the status bar says, nothing else. A signed file without a licensee
means the same (see private-use).

## format

A plain UTF-8 text file, one `Field: value` per line, closed by the `Signature:` line. The
signed payload is every line before the signature, each ended by a single `\n` whatever the
line ending in the file was, so a file that went through a Windows editor still verifies.
Fields other than `Licensee` are carried along in the payload, so more of them can be added
later without breaking the signature of older files. The signature is hex, not base64: it is
64 bytes, and hex needs no decoder that can disagree about padding.

## private-use

A licence without a licensee - "Licensee:" left empty, still signed - is a valid licence for
private use, and the program shows it as such, the same as no file at all. It lets a private
user hold a signed file too, so the licence file's presence alone never decides anything.

## read

A missing file is not an event; an unreadable or tampered one is logged as a warning with the
path, because the licensee will want to know why the program calls itself a private-use
installation.

## find

The first directory that holds a licence file wins: the configuration directory before the
directory of the executable, so a user can license an installation without write access to
Program Files. A found file that does not verify does not fall through to the next directory -
a broken licence is something to fix, not to hide behind another one.

## verify

OpenSSL's one-shot EVP_DigestVerify with an Ed25519 key does the hashing itself; there is no
separate digest to choose, which is one of the reasons for Ed25519.

## sign_for_test

Makes a fresh key pair and signs the payload with it, handing the public key back: the tests
must not depend on the product's private key, which is not in the repository. A real licence
is made with tools/makeLicense.sh.

## PRODUCT_PUBLIC_KEY

The raw 32 bytes of the product's public key, the counterpart of the private key in the
developer's `autofilesync-license` directory outside the repository. Replacing it invalidates
every licence issued so far.
