#!/bin/bash
# Builds the signed installation file autofilesync-<version>.apk of the Android app and a
# .sha256 file next to it. The version comes from versionName in AFS_Android/app/build.gradle.kts.
set -e
umask 022

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
ANDROID_DIR="${SOURCE_DIR}/AFS_Android"
GRADLE_FILE="${ANDROID_DIR}/app/build.gradle.kts"
BUILT_APK="${ANDROID_DIR}/app/build/outputs/apk/release/app-release.apk"
OUTPUT_DIR="${SOURCE_DIR}/build"
SKIP_BUILD=false
INSTALL=false
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

info() { echo -e "${GREEN}$*${NC}"; }
warn() { echo -e "${YELLOW}$*${NC}" >&2; }
fail() { echo -e "${RED}$*${NC}" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) SKIP_BUILD=true; shift ;;
        --install) INSTALL=true; shift ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN]"
            echo "  --no-build      Vorhandene app-release.apk verwenden"
            echo "  --install       Die APK danach per adb auf das angeschlossene Handy installieren"
            echo "  --output DIR    Ablageort der APK (Vorgabe: build/)"
            echo "  -h, --help      Diese Hilfe"
            echo ""
            echo "Die Version steht in versionName/versionCode in AFS_Android/app/build.gradle.kts."
            echo "Signiert wird mit dem Schlüssel aus AFS_Android/keystore.properties"
            echo "(einmalig erzeugen mit AFS_Android/tools/make-release-key.sh)."
            exit 0 ;;
        *) fail "Unbekannte Option: $1" ;;
    esac
done

gradle_value() { sed -n "s/^ *$1 = \"\\?\\([^\"]*\\)\"\\?$/\\1/p" "${GRADLE_FILE}" | head -1; }

sdk_dir()
{
    local dir
    dir="$(sed -n 's/^sdk\.dir=//p' "${ANDROID_DIR}/local.properties" 2>/dev/null)"
    echo "${dir:-${ANDROID_HOME:-${ANDROID_SDK_ROOT:-${HOME}/Android/Sdk}}}"
}

apksigner_path()
{
    local tool
    tool="$(ls -d "$(sdk_dir)"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
    [ -x "${tool}" ] || fail "apksigner nicht gefunden - Android SDK build-tools installieren und sdk.dir in AFS_Android/local.properties setzen."
    echo "${tool}"
}

check_prerequisites()
{
    info "=== Voraussetzungen prüfen ==="
    [ -f "${ANDROID_DIR}/keystore.properties" ] \
        || fail "AFS_Android/keystore.properties fehlt - ohne Release-Schlüssel entsteht keine installierbare APK (AFS_Android/tools/make-release-key.sh)."
    command -v java &>/dev/null || fail "java fehlt (Java 17 oder neuer)."
    command -v sha256sum &>/dev/null || fail "sha256sum fehlt."
    [[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "versionName „${VERSION}“ in app/build.gradle.kts ist nicht major.minor.patch."
    [[ "${VERSION_CODE}" =~ ^[0-9]+$ ]] || fail "versionCode „${VERSION_CODE}“ in app/build.gradle.kts ist keine Zahl."
    [ -f "${ANDROID_DIR}/fastlane/metadata/android/en-US/changelogs/${VERSION_CODE}.txt" ] \
        || warn "fastlane/metadata/android/en-US/changelogs/${VERSION_CODE}.txt fehlt (Änderungsliste für F-Droid)."
}

build_release()
{
    info "=== APK bauen ==="
    (cd "${ANDROID_DIR}" && ./gradlew --quiet clean assembleRelease)
}

verify_signature()
{
    info "=== Signatur prüfen ==="
    [ -f "${BUILT_APK}" ] || fail "${BUILT_APK} fehlt - ohne --no-build aufrufen."
    "$(apksigner_path)" verify --print-certs "${BUILT_APK}" | grep -m1 'SHA-256 digest' \
        || fail "Die APK ist nicht gültig signiert."
}

install_on_phone()
{
    info "=== Auf dem Handy installieren ==="
    command -v adb &>/dev/null || fail "adb fehlt."
    adb install -r "$1" \
        || fail "Installation fehlgeschlagen. Ist noch eine Debug-Version installiert, erst deinstallieren: adb uninstall com.welschsoftware.autofilesync"
}

VERSION="$(gradle_value versionName)"
VERSION_CODE="$(gradle_value versionCode)"
check_prerequisites
info "=== Version ${VERSION} (versionCode ${VERSION_CODE}) ==="
[ "${SKIP_BUILD}" = true ] || build_release
verify_signature

mkdir -p "${OUTPUT_DIR}"
APK="${OUTPUT_DIR}/autofilesync-${VERSION}.apk"
cp "${BUILT_APK}" "${APK}"
(cd "${OUTPUT_DIR}" && sha256sum "$(basename "${APK}")" > "$(basename "${APK}").sha256")
[ "${INSTALL}" = true ] && install_on_phone "${APK}"

info "=== Fertig ==="
echo "APK: ${APK} ($(du -h "${APK}" | cut -f1))"
echo "Prüfsumme: ${APK}.sha256"
echo "Installieren: adb install -r ${APK}  oder die Datei aufs Handy kopieren und öffnen"
