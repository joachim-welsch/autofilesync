#!/bin/sh
# Creates the release signing key of the AutoFileSync app and keystore.properties next to the
# Gradle project. Run once. Keep the .jks file and its passwords safe and backed up: without
# them no update of the app can ever be installed over a released version.
set -eu
cd "$(dirname "$0")/.."
STORE="${1:-$HOME/.android/autofilesync-release.jks}"
[ -e "$STORE" ] && { echo "$STORE exists already - not overwritten."; exit 1; }
printf 'Password for the key (at least 6 characters): '
stty -echo; read -r PASSWORD; stty echo; echo
keytool -genkeypair -v -keystore "$STORE" -alias autofilesync -keyalg RSA -keysize 4096 -validity 10000 \
    -storepass "$PASSWORD" -keypass "$PASSWORD" -dname "CN=AutoFileSync, O=Joachim Welsch, C=DE"
umask 077
cat > keystore.properties <<PROPS
storeFile=$STORE
storePassword=$PASSWORD
keyAlias=autofilesync
keyPassword=$PASSWORD
PROPS
echo "Done. Back up $STORE and the password. Release APK: ./gradlew assembleRelease"
