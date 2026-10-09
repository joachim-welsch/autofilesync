#!/bin/bash
# Builds the AutoFileSync disk image that end users install from: a signed,
# self-contained AutoFileSync.app next to a link to the Programs folder.
set -e
source "$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )/AFS_Desktop/installer/macos_bundle.sh"

BUILD_DIR="${AFS_SOURCE_DIR}/build/Release"
OUTPUT_DIR="${AFS_SOURCE_DIR}/build"
VOLUME_NAME="AutoFileSync"
SKIP_BUILD=false
REQUIRE_LAYOUT=false
# Must match the layout the background artwork was drawn for
# (AFS_Desktop/installer/make_dmg_background.swift).
WINDOW_WIDTH=600; WINDOW_HEIGHT=400; ICON_ROW=205; SOURCE_ICON_X=150; TARGET_ICON_X=450; ICON_SIZE=128

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) SKIP_BUILD=true; shift ;;
        --require-layout) REQUIRE_LAYOUT=true; shift ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN]"
            echo "  --no-build      Vorhandenes Bündel in build/Release verwenden"
            echo "  --output DIR    Ablageort des Disk-Images (Vorgabe: build/)"
            echo "  --require-layout  Abbrechen, wenn das Fensterlayout nicht gesetzt werden kann"
            echo "  -h, --help      Diese Hilfe"
            echo ""
            echo "Umgebung:"
            echo "  AFS_CODESIGN_IDENTITY   Signaturidentität (Vorgabe: ad hoc)"
            exit 0 ;;
        *) afs_fail "Unbekannte Option: $1" ;;
    esac
done

stage_contents() {
    local staging="$1" app="$2"
    rm -rf "${staging}"; mkdir -p "${staging}/.background"
    cp -R "${app}" "${staging}/${VOLUME_NAME}.app"
    ln -s /Applications "${staging}/Applications"
    cp "${AFS_DESKTOP_DIR}/installer/dmg-background.tiff" "${staging}/.background/background.tiff"
}

create_writable_image() {
    local staging="$1" image="$2"
    local megabytes=$(( $(du -sk "${staging}" | cut -f1) / 1024 + 50 ))  # slack for the Finder metadata written while laying out
    rm -f "${image}"
    hdiutil create -srcfolder "${staging}" -volname "${VOLUME_NAME}" -fs HFS+ \
        -format UDRW -size "${megabytes}m" "${image}" >/dev/null
}

# Gives the mounted disk the application icon instead of the generic one. Runs
# after the layout because Finder discards the icon file while it rewrites the
# window settings. SetFile ships with Xcode only; the extended attribute is the
# same flag written by hand.
apply_volume_icon() {
    local volume="$1"
    cp "${AFS_DESKTOP_DIR}/icons/afs.icns" "${volume}/.VolumeIcon.icns"
    if command -v SetFile &>/dev/null; then SetFile -a C "${volume}"
    else xattr -wx com.apple.FinderInfo \
        "0000000000000000040000000000000000000000000000000000000000000000" "${volume}"
    fi
}

# Finder is the only writer of the .DS_Store that carries icon positions and the
# background picture, and driving it needs the one-time automation permission
# (Systemeinstellungen > Datenschutz & Sicherheit > Automation). Without it the
# disk image still works - dragging the application onto the link does not
# depend on the layout - so this step must never abort the build unless the
# caller asked for a release-quality image with --require-layout.
apply_finder_layout() {
    local volume_name="$1"
    osascript - "$volume_name" <<APPLESCRIPT
on run argv
    set volumeName to item 1 of argv
    with timeout of 120 seconds
        tell application "Finder"
            tell disk volumeName
                open
                set current view of container window to icon view
                set toolbar visible of container window to false
                set statusbar visible of container window to false
                set the bounds of container window to {200, 120, ${WINDOW_WIDTH} + 200, ${WINDOW_HEIGHT} + 120}
                set viewOptions to the icon view options of container window
                set arrangement of viewOptions to not arranged
                set icon size of viewOptions to ${ICON_SIZE}
                set background picture of viewOptions to file ".background:background.tiff"
                set position of item "${VOLUME_NAME}.app" of container window to {${SOURCE_ICON_X}, ${ICON_ROW}}
                set position of item "Applications" of container window to {${TARGET_ICON_X}, ${ICON_ROW}}
                close
                open
                update without registering applications
                delay 2
                close
            end tell
        end tell
    end timeout
end run
APPLESCRIPT
}

report_layout_failure() {
    afs_warn "Das Fensterlayout konnte nicht gesetzt werden."
    afs_warn "  Das Disk-Image bleibt benutzbar, zeigt aber die Standardanordnung ohne Hintergrundbild."
    afs_warn "  Abhilfe: Systemeinstellungen > Datenschutz & Sicherheit > Automation >"
    afs_warn "  das aufrufende Programm (Terminal) fuer \"Finder\" freigeben und erneut ausfuehren."
    [ "${REQUIRE_LAYOUT}" = true ] && afs_fail "Abbruch wegen --require-layout."
    return 0
}

lay_out_image() {
    local image="$1" volume
    volume="$(hdiutil attach -readwrite -noverify -noautoopen "${image}" | grep '/Volumes/' | sed 's/.*\(\/Volumes\/.*\)/\1/')"
    [ -d "${volume}" ] || afs_fail "Disk-Image konnte nicht eingehängt werden."
    apply_finder_layout "$(basename "${volume}")" >/dev/null 2>&1 || report_layout_failure
    apply_volume_icon "${volume}"
    sync
    hdiutil detach "${volume}" >/dev/null || hdiutil detach "${volume}" -force >/dev/null
}

compress_image() {
    local writable="$1" final="$2"
    rm -f "${final}"
    hdiutil convert "${writable}" -format UDZO -imagekey zlib-level=9 -o "${final}" >/dev/null
    rm -f "${writable}"
    [ -n "${AFS_CODESIGN_IDENTITY}" ] && codesign --force --sign "${AFS_CODESIGN_IDENTITY}" "${final}"
    hdiutil verify "${final}" >/dev/null || afs_fail "Disk-Image ist beschädigt."
}

if [ "${SKIP_BUILD}" = true ]; then
    afs_verify_bundle "${BUILD_DIR}/autofilesync.app"
else
    afs_make_bundle "${BUILD_DIR}"
fi

afs_info "=== Disk-Image erzeugen ==="
mkdir -p "${OUTPUT_DIR}"
STAGING="${BUILD_DIR}/dmg_staging"
WRITABLE="${BUILD_DIR}/${VOLUME_NAME}-writable.dmg"
FINAL="${OUTPUT_DIR}/${VOLUME_NAME}-$(afs_version).dmg"
stage_contents "${STAGING}" "${BUILD_DIR}/autofilesync.app"
create_writable_image "${STAGING}" "${WRITABLE}"
rm -rf "${STAGING}"
lay_out_image "${WRITABLE}"
compress_image "${WRITABLE}" "${FINAL}"

afs_info "=== Fertig ==="
echo "Installationsprogramm: ${FINAL} ($(du -h "${FINAL}" | cut -f1))"
