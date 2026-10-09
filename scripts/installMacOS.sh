#!/bin/bash
# Builds AutoFileSync and installs the finished bundle into /Applications.
# For a disk image to hand to other people use scripts/makeMacOSInstaller.sh.
set -e
source "$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )/AFS_Desktop/installer/macos_bundle.sh"

BUILD_DIR="${AFS_SOURCE_DIR}/build/Release"
INSTALL_DIR="/Applications"
INSTALLED_APP="${INSTALL_DIR}/AutoFileSync.app"

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-install) INSTALL_DIR=""; shift ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN]"
            echo "  --no-install   Nur bauen, nicht nach /Applications kopieren"
            echo "  -h, --help     Diese Hilfe"
            echo ""
            echo "Ein verteilbares Disk-Image erzeugt scripts/makeMacOSInstaller.sh."
            exit 0 ;;
        *) afs_fail "Unbekannte Option: $1" ;;
    esac
done

afs_make_bundle "${BUILD_DIR}"

[ -z "${INSTALL_DIR}" ] && { afs_warn "Installation übersprungen (--no-install)."; exit 0; }

afs_info "=== Nach ${INSTALL_DIR} installieren ==="
# A running copy keeps its old code pages mapped; replacing it underneath is
# what produces the "application is damaged" reports after an update.
pkill -x autofilesync 2>/dev/null && sleep 1 || true
rm -rf "${INSTALLED_APP}"
cp -R "${BUILD_DIR}/autofilesync.app" "${INSTALLED_APP}"
afs_info "AutoFileSync ist unter ${INSTALLED_APP} installiert."
