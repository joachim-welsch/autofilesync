#!/bin/sh
# Prints the assetlinks.json for https://welsch-software.com/.well-known/assetlinks.json from the
# release key in keystore.properties. With it, Android opens the pairing link of the QR code
# directly in the app. Add further fingerprints (e.g. F-Droid's signing key) to the array by hand.
set -eu
cd "$(dirname "$0")/.."
. ./keystore.properties 2>/dev/null || { echo "keystore.properties missing - run tools/make-release-key.sh first" >&2; exit 1; }
FINGERPRINT=$(keytool -list -v -keystore "$storeFile" -alias "$keyAlias" -storepass "$storePassword" | sed -n 's/.*SHA256: *//p' | head -1)
cat <<JSON
[{
  "relation": ["delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "android_app",
    "package_name": "com.welschsoftware.autofilesync",
    "sha256_cert_fingerprints": ["$FINGERPRINT"]
  }
}]
JSON
