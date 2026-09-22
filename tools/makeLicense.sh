#!/bin/bash
# Makes a licence file for AutoFileSync: autofilesync.lic, signed with the product's
# Ed25519 private key (kept outside the repository, see AFS_Core/src/License.md).
#   tools/makeLicense.sh <private key .pem> "<licensee>" [output file]
set -e
if [ $# -lt 2 ]; then
    echo "usage: $0 <private key .pem> \"<licensee>\" [output file]" >&2
    exit 1
fi
key="$1"
licensee="$2"
out="${3:-autofilesync.lic}"
payload="$(mktemp)"
trap 'rm -f "$payload"' EXIT
printf 'Licensee: %s\n' "$licensee" > "$payload"
signature="$(openssl pkeyutl -sign -inkey "$key" -rawin -in "$payload" | xxd -p -c 256)"
{ cat "$payload"; printf 'Signature: %s\n' "$signature"; } > "$out"
echo "written: $out"
