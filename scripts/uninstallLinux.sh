#!/bin/bash
# Removes an installed AutoFileSync: the Debian package as well as a manual
# installation following doc/INSTALL-Linux.md or backgroundsync/install.sh.
set -euo pipefail

PURGE=false
MANUAL_ONLY=false
DRY_RUN=false
ASSUME_YES=false
UNIT=backgroundsync.service
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/autofilesync"
AUTOSTART_FILE="${HOME}/.config/autostart/my_program.desktop"
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

info() { echo -e "${GREEN}$*${NC}"; }
warn() { echo -e "${YELLOW}$*${NC}" >&2; }
fail() { echo -e "${RED}$*${NC}" >&2; exit 1; }
run() { echo "+ $*"; [ "${DRY_RUN}" = true ] || "$@"; }

usage()
{
    echo "Aufruf: $0 [OPTIONEN]"
    echo "  --purge       Zusätzlich Konfiguration, Schlüssel und Protokoll des Benutzers,"
    echo "                die Dienst-Benutzerdatei und das Systemkonto backgroundsync entfernen"
    echo "  --manual-only Nur Reste einer manuellen Installation entfernen; Paket, Dienststatus"
    echo "                und Benutzerdaten bleiben unberührt (vor der Installation des Pakets)"
    echo "  --dry-run     Nur anzeigen, was entfernt würde"
    echo "  -y, --yes     Keine Rückfrage"
    echo "  -h, --help    Diese Hilfe"
    echo ""
    echo "Ohne --purge bleiben die Benutzerdaten in ${CONFIG_DIR} erhalten."
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --purge) PURGE=true; shift ;;
        --manual-only) MANUAL_ONLY=true; shift ;;
        --dry-run) DRY_RUN=true; shift ;;
        -y|--yes) ASSUME_YES=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "Unbekannte Option: $1" ;;
    esac
done

[ "$(id -u)" -ne 0 ] || fail "Bitte als normaler Benutzer aufrufen - sudo wird bei Bedarf selbst angefordert."  # as root, HOME would point at root's configuration instead of the user's

package_state()
{
    dpkg-query -W -f='${db:Status-Status}' autofilesync 2>/dev/null || true
}

confirm()
{
    [ "${ASSUME_YES}" = true ] || [ "${DRY_RUN}" = true ] && return 0
    local answer
    read -r -p "$1 [j/N] " answer
    [[ "${answer}" =~ ^[jJyY]$ ]]
}

stop_programs()
{
    info "=== Programm und Hintergrunddienst beenden ==="
    pgrep -x autofilesync >/dev/null && run pkill -x autofilesync || true
    if [ -d /run/systemd/system ] && systemctl list-unit-files "${UNIT}" 2>/dev/null | grep -q "^${UNIT}"; then
        run sudo systemctl disable --now "${UNIT}" || true
    fi
}

remove_package()
{
    local state; state="$(package_state)"
    [ -n "${state}" ] && [ "${state}" != not-installed ] || return 0
    info "=== Debian-Paket entfernen (${state}) ==="
    if [ "${PURGE}" = true ]; then
        run sudo apt-get purge -y autofilesync
    elif [ "${state}" = installed ]; then
        run sudo apt-get remove -y autofilesync
    fi
}

exists()
{
    [ -e "$1" ] || [ -L "$1" ] || { [ ! -x "$(dirname "$1")" ] && sudo test -e "$1"; }  # /etc/polkit-1/rules.d is not searchable for normal users
}

remove_if_present()
{
    local path present=()
    for path in "$@"; do
        exists "${path}" && present+=("${path}")
    done
    [ ${#present[@]} -eq 0 ] || run sudo rm -rf "${present[@]}"
}

# Whatever is left after the package removal stems from a manual installation;
# in a dry run the package is still there, so its own files are skipped.
manual_leftovers()
{
    local path
    for path in /etc/systemd/system/"${UNIT}" /usr/sbin/backgroundsync /usr/share/locale/*/LC_MESSAGES/autofilesync.mo \
        /opt/autofilesync /usr/local/bin/autofilesync /usr/local/share/applications/autofilesync.desktop \
        /usr/local/share/icons/hicolor/*/apps/autofilesync.png /etc/polkit-1/rules.d/50-backgroundsync.rules; do
        dpkg-query -S "${path}" &>/dev/null || echo "${path}"
    done
}

remove_manual_installation()
{
    info "=== Reste einer manuellen Installation entfernen ==="
    local leftovers; mapfile -t leftovers < <(manual_leftovers)
    remove_if_present "${leftovers[@]}"
    command -v gtk-update-icon-cache &>/dev/null && [ -d /usr/local/share/icons/hicolor ] \
        && run sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor 2>/dev/null || true
    command -v update-desktop-database &>/dev/null && [ -d /usr/local/share/applications ] \
        && run sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true
    [ -d /run/systemd/system ] && run sudo systemctl daemon-reload || true
}

remove_autostart()
{
    [ -f "${AUTOSTART_FILE}" ] && grep -q '^Exec=.*autofilesync' "${AUTOSTART_FILE}" || return 0  # the generic file name may belong to another program
    info "=== Autostart-Eintrag entfernen ==="
    run rm -f "${AUTOSTART_FILE}"
}

purge_user_data()
{
    info "=== Benutzerdaten und Dienstkonto entfernen ==="
    remove_if_present /etc/systemd/system/"${UNIT}".d
    getent passwd backgroundsync >/dev/null && run sudo userdel backgroundsync || true  # account from backgroundsync/setup.sh
    getent group backgroundsync >/dev/null && run sudo groupdel backgroundsync || true
    [ -d "${CONFIG_DIR}" ] || return 0
    warn "${CONFIG_DIR}/secret.key entschlüsselt die gespeicherten SSH-Kennwörter - nach dem Löschen müssen alle SSH-Konten neu eingerichtet werden."
    if confirm "${CONFIG_DIR} löschen?"; then
        run rm -rf "${CONFIG_DIR}"
    else
        warn "${CONFIG_DIR} bleibt erhalten."
    fi
}

summary()
{
    local state; state="$(package_state)"
    echo "Paketstatus: ${state:-nicht vorhanden}"
    echo "Programm im Suchpfad: $(command -v autofilesync || echo keines)"
    [ "${PURGE}" = true ] || [ ! -d "${CONFIG_DIR}" ] || echo "Benutzerdaten erhalten: ${CONFIG_DIR} (mit --purge entfernen)"
}

[ "${DRY_RUN}" = true ] && warn "Probelauf - es wird nichts verändert."
[ "${MANUAL_ONLY}" = false ] || [ "${PURGE}" = false ] || fail "--manual-only und --purge schließen sich aus."
[ "${MANUAL_ONLY}" = false ] || { remove_manual_installation; exit 0; }
confirm "AutoFileSync deinstallieren?" || fail "Abgebrochen."
stop_programs
remove_package
remove_manual_installation
remove_autostart
[ "${PURGE}" = false ] || purge_user_data
info "=== Fertig ==="
summary
