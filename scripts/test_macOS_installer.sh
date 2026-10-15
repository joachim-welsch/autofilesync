#!/bin/bash
# Tests the disk image built by makeMacOSInstaller.sh: that it mounts and is
# laid out for dragging, that the bundle inside is completely described and
# self-contained, that a drag to the programs folder yields an installation
# that keeps working once the image is ejected, and that starting it leaves the
# user's configuration untouched.
source "$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )/AFS_Desktop/installer/macos_bundle.sh"

DMG=""
RUN_LAUNCH_TEST=true
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-launch) RUN_LAUNCH_TEST=false; shift ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN] [DISKIMAGE]"
            echo "  --no-launch    Das Programm nicht probeweise starten"
            echo "  -h, --help     Diese Hilfe"
            echo ""
            echo "Ohne Angabe wird das neueste Image in build/ geprüft."
            exit 0 ;;
        *) DMG="$1"; shift ;;
    esac
done
[ -n "${DMG}" ] || DMG="$(ls -t "${AFS_SOURCE_DIR}"/build/AutoFileSync-*.dmg 2>/dev/null | head -1)"
[ -f "${DMG}" ] || afs_fail "Kein Disk-Image gefunden - zuerst scripts/makeMacOSInstaller.sh ausführen."

PASSED=0; FAILED=0
ok()   { echo -e "  ${AFS_GREEN}BESTANDEN${AFS_NC}  $1"; PASSED=$((PASSED+1)); }
bad()  { echo -e "  ${AFS_RED}FEHLER${AFS_NC}     $1${2:+ - $2}"; FAILED=$((FAILED+1)); }
hint() { echo -e "  ${AFS_YELLOW}HINWEIS${AFS_NC}    $1${2:+ - $2}"; }
section() { echo ""; afs_info "=== $1 ==="; }
expect() { local description="$1"; shift; local detail; if detail="$("$@" 2>&1)"; then ok "$description"; else bad "$description" "$(echo "$detail" | head -2 | tr '\n' ' ')"; fi }
expect_equal() { [ "$2" = "$3" ] && ok "$1" || bad "$1" "erwartet '$2', gefunden '$3'"; }
expect_file() { [ -f "$2" ] && ok "$1" || bad "$1" "$2 fehlt"; }

WORK_DIR="$(mktemp -d)"
MOUNTPOINT=""
cleanup() {
    [ -n "${MOUNTPOINT}" ] && [ -d "${MOUNTPOINT}" ] && hdiutil detach "${MOUNTPOINT}" -force >/dev/null 2>&1
    rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

echo "Geprüftes Disk-Image: ${DMG}"

section "Disk-Image"
expect "Image ist unbeschädigt" hdiutil verify "${DMG}"
MOUNTPOINT="$(hdiutil attach -readonly -nobrowse -noautoopen "${DMG}" | grep -o '/Volumes/.*$' | tail -1)"
[ -d "${MOUNTPOINT}" ] || afs_fail "Image konnte nicht eingehängt werden."
ok "Image lässt sich einhängen (${MOUNTPOINT})"
expect_equal "Datenträger heißt AutoFileSync" "AutoFileSync" "$(basename "${MOUNTPOINT}")"
MOUNTED_APP="${MOUNTPOINT}/AutoFileSync.app"
[ -d "${MOUNTED_APP}" ] && ok "Enthält AutoFileSync.app" || bad "Enthält AutoFileSync.app"
expect_equal "Verknüpfung auf den Programme-Ordner" "/Applications" "$(readlink "${MOUNTPOINT}/Applications")"
expect_file "Hintergrundbild für das Fenster" "${MOUNTPOINT}/.background/background.tiff"
# The icons have to land on the arrow the background was drawn around, so the
# positions Finder stored are compared against the artwork's, not just found.
icon_positions() {
    python3 -c "
import struct, sys
data = open(sys.argv[1], 'rb').read()
positions, at = [], 0
while True:
    at = data.find(b'Iloc', at)
    if at < 0: break
    positions.append('%d,%d' % struct.unpack('>ii', data[at + 12:at + 20]))
    at += 4
print(' '.join(sorted(positions)))" "${MOUNTPOINT}/.DS_Store" 2>/dev/null
}
if [ -n "$(icon_positions)" ]; then
    expect_equal "Symbole liegen auf dem Pfeil des Hintergrundbilds" "150,205 450,205" "$(icon_positions)"
    strings -a "${MOUNTPOINT}/.DS_Store" | grep -q background.tiff \
        && ok "Hintergrundbild ist im Fenster gesetzt" || bad "Hintergrundbild ist im Fenster nicht gesetzt"
else
    hint "Fensterlayout fehlt" "nur kosmetisch; Automation-Recht fuer Finder noetig"
fi
expect_file "Datenträger hat ein eigenes Symbol" "${MOUNTPOINT}/.VolumeIcon.icns"

section "Beschreibung des Programmbündels"
plist_value() { /usr/libexec/PlistBuddy -c "Print :$1" "${MOUNTED_APP}/Contents/Info.plist" 2>/dev/null; }
expect_equal "Bündelkennung" "de.autofilesync.app" "$(plist_value CFBundleIdentifier)"
expect_equal "Programmdatei" "autofilesync" "$(plist_value CFBundleExecutable)"
expect_equal "Angezeigter Name" "AutoFileSync" "$(plist_value CFBundleDisplayName)"
expect_equal "Symboldatei" "afs" "$(plist_value CFBundleIconFile)"
expect_equal "Version stammt aus Version.h" "$(afs_version)" "$(plist_value CFBundleShortVersionString)"
expect_equal "Mindestsystem" "13.3" "$(plist_value LSMinimumSystemVersion)"

section "Inhalt des Programmbündels"
expect_file "Programmsymbol" "${MOUNTED_APP}/Contents/Resources/afs.icns"
expect_file "Hilfebuch neben dem Programm" "${MOUNTED_APP}/Contents/Resources/help/help.htb"
expect_file "Englisches Hilfebuch neben dem Programm" "${MOUNTED_APP}/Contents/Resources/help/en/help.htb"
expect_equal "Programmdatei ist ausführbar" "yes" \
    "$([ -x "${MOUNTED_APP}/Contents/MacOS/autofilesync" ] && echo yes || echo no)"
expect_equal "Passend zur Architektur dieses Rechners" "yes" \
    "$(lipo -archs "${MOUNTED_APP}/Contents/MacOS/autofilesync" 2>/dev/null | grep -q "$(uname -m)" && echo yes || echo no)"
expect_equal "Hintergrunddienst liegt neben dem Programm" "yes" \
    "$([ -x "${MOUNTED_APP}/Contents/MacOS/backgroundsync" ] && echo yes || echo no)"
expect "Hintergrunddienst ist signiert" codesign --verify --strict "${MOUNTED_APP}/Contents/MacOS/backgroundsync"
LEAKED=""
for binary in $(afs_executables "${MOUNTED_APP}") "${MOUNTED_APP}"/Contents/Frameworks/*.dylib; do
    external="$(afs_external_deps "$binary")"
    [ -z "$external" ] || LEAKED="${LEAKED} $(basename "$binary"):$(echo "$external" | tr '\n' ',')"
done
[ -z "${LEAKED}" ] && ok "Keine Bibliothek außerhalb des Bündels ($(ls "${MOUNTED_APP}"/Contents/Frameworks/*.dylib | wc -l | tr -d ' ') eingebettet)" \
    || bad "Bibliotheken außerhalb des Bündels" "${LEAKED}"
DUPLICATES="$(shasum "${MOUNTED_APP}"/Contents/Frameworks/*.dylib | awk '{print $1}' | sort | uniq -d | wc -l | tr -d ' ')"
expect_equal "Keine Bibliothek doppelt eingebettet" "0" "${DUPLICATES}"
expect "Signatur des Bündels ist gültig" codesign --verify --deep --strict "${MOUNTED_APP}"

section "Installation durch Ziehen"
INSTALL_TARGET="${WORK_DIR}/Applications"
mkdir -p "${INSTALL_TARGET}"
expect "Bündel lässt sich in den Programme-Ordner kopieren" cp -R "${MOUNTED_APP}" "${INSTALL_TARGET}/AutoFileSync.app"
INSTALLED_APP="${INSTALL_TARGET}/AutoFileSync.app"
hdiutil detach "${MOUNTPOINT}" >/dev/null 2>&1 || hdiutil detach "${MOUNTPOINT}" -force >/dev/null 2>&1
MOUNTPOINT=""
ok "Image ausgeworfen - die Installation muss jetzt allein bestehen"
expect "Signatur überlebt das Kopieren" codesign --verify --deep --strict "${INSTALLED_APP}"
MISSING=""
for binary in $(afs_executables "${INSTALLED_APP}"); do
    for dependency in $(otool -L "${binary}" | awk 'NR>1 {print $1}' | grep '^@executable_path'); do
        resolved="${INSTALLED_APP}/Contents/MacOS/${dependency#@executable_path/}"
        [ -f "${resolved}" ] || MISSING="${MISSING} $(basename "${binary}"):${dependency}"
    done
done
[ -z "${MISSING}" ] && ok "Alle eingebetteten Bibliotheken sind auflösbar" || bad "Fehlende Bibliotheken" "${MISSING}"

section "Wiederholtes Einbetten"
# The bundling step rewrites the executable's load commands, so running it again
# sees a bundle that no longer names its original libraries. It has to cope with
# that: a second installer run must not strip the bundle.
REBUNDLED="${WORK_DIR}/rebundled.app"
cp -R "${INSTALLED_APP}" "${REBUNDLED}"
if afs_bundle_dylibs "${REBUNDLED}" >/dev/null 2>&1 && afs_codesign_app "${REBUNDLED}" >/dev/null 2>&1 \
   && afs_verify_bundle "${REBUNDLED}" >/dev/null 2>&1; then
    ok "Zweiter Durchlauf liefert wieder ein vollständiges Bündel"
else
    bad "Zweiter Durchlauf beschädigt das Bündel"
fi
expect_equal "Gleiche Bibliotheken wie im ersten Durchlauf" \
    "$(ls "${INSTALLED_APP}/Contents/Frameworks")" "$(ls "${REBUNDLED}/Contents/Frameworks")"

if [ "${RUN_LAUNCH_TEST}" = false ]; then
    section "Programmstart"
    hint "Übersprungen (--no-launch)"
else
    section "Programmstart aus der Installation"
    # A private HOME keeps the trial run out of the real configuration: the
    # program derives its settings, log and window state from $HOME alone.
    SANDBOX_HOME="${WORK_DIR}/home"
    mkdir -p "${SANDBOX_HOME}"
    fingerprint() { find "${HOME}/Library/Application Support/autofilesync" "${HOME}/.config/autofilesync" \
        -type f -exec shasum {} \; 2>/dev/null | sort; }
    BEFORE="$(fingerprint)"
    HOME="${SANDBOX_HOME}" "${INSTALLED_APP}/Contents/MacOS/autofilesync" >"${WORK_DIR}/stdout.log" 2>"${WORK_DIR}/stderr.log" &
    APP_PID=$!
    sleep 8
    if kill -0 "${APP_PID}" 2>/dev/null; then
        ok "Programm läuft nach 8 Sekunden noch"
        WINDOWS="$(osascript -e "tell application \"System Events\" to count windows of (first process whose unix id is ${APP_PID})" 2>/dev/null)"
        [ -n "${WINDOWS}" ] && [ "${WINDOWS}" -gt 0 ] 2>/dev/null && ok "Ein Fenster ist geöffnet (${WINDOWS})" \
            || hint "Fensterzahl nicht feststellbar" "Terminal braucht das Recht Bedienungshilfen"
        kill "${APP_PID}" 2>/dev/null
        wait "${APP_PID}" 2>/dev/null
        ok "Programm ließ sich beenden"
    else
        wait "${APP_PID}" 2>/dev/null
        bad "Programm startet nicht" "$(tail -3 "${WORK_DIR}/stderr.log" | tr '\n' ' ')"
    fi
    if grep -q "is implemented in both" "${WORK_DIR}/stderr.log" 2>/dev/null; then
        bad "Laufzeit meldet doppelt geladene Klassen" "$(grep -m1 -o "Class [A-Za-z_]*" "${WORK_DIR}/stderr.log")"
    else
        ok "Keine doppelt geladenen Klassen zur Laufzeit"
    fi
    [ -s "${WORK_DIR}/stderr.log" ] && hint "Meldungen auf stderr" "$(head -2 "${WORK_DIR}/stderr.log" | cut -c1-160 | tr '\n' ' ')"
    [ -n "$(find "${SANDBOX_HOME}" -type f 2>/dev/null)" ] && ok "Programm legt seine Dateien unter \$HOME an" \
        || hint "Keine Dateien im Testverzeichnis" "das Programm kam womöglich nicht bis zum Speichern"
    expect_equal "Konfiguration des Benutzers unverändert" "${BEFORE}" "$(fingerprint)"

    section "Hintergrunddienst aus der Installation"
    # The daemon runs with the same private HOME: it reads no configuration
    # there, logs that, and must stay alive - that is what launchd expects of it.
    HOME="${SANDBOX_HOME}" "${INSTALLED_APP}/Contents/MacOS/backgroundsync" >"${WORK_DIR}/daemon.log" 2>&1 &
    DAEMON_PID=$!
    sleep 3
    if kill -0 "${DAEMON_PID}" 2>/dev/null; then
        ok "Hintergrunddienst läuft nach 3 Sekunden noch"
        kill "${DAEMON_PID}" 2>/dev/null; wait "${DAEMON_PID}" 2>/dev/null
    else
        wait "${DAEMON_PID}" 2>/dev/null
        bad "Hintergrunddienst startet nicht" "$(tail -3 "${WORK_DIR}/daemon.log" | tr '\n' ' ')"
    fi
    grep -q " D " "${SANDBOX_HOME}/.cache/autofilesync/autofilesync.log" 2>/dev/null \
        && ok "Hintergrunddienst kennzeichnet seine Logzeilen mit D" \
        || hint "Keine Logzeile des Hintergrunddienstes gefunden"
fi

section "Ergebnis"
echo "  ${PASSED} bestanden, ${FAILED} fehlgeschlagen"
[ "${FAILED}" -eq 0 ] || afs_fail "Der Test des Installationsprogramms ist fehlgeschlagen."
afs_info "Das Installationsprogramm ist in Ordnung."
