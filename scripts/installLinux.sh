#!/bin/bash
# Installs the newest AutoFileSync release as Debian package: builds the package
# from the highest release tag v<major>.<minor>.<patch> and installs it with apt.
set -euo pipefail

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
RELEASE_SOURCE="${SOURCE_DIR}/build/release-source"  # kept between runs, so later releases build incrementally
PACKAGE_DIR="${SOURCE_DIR}/build/release-package"
UNIT=backgroundsync.service
DROPIN=/etc/systemd/system/${UNIT}.d/user.conf
FROM_HEAD=false
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

info() { echo -e "${GREEN}$*${NC}"; }
warn() { echo -e "${YELLOW}$*${NC}" >&2; }
fail() { echo -e "${RED}$*${NC}" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --head) FROM_HEAD=true; shift ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN]"
            echo "  --head        Den aktuellen Stand dieses Arbeitsverzeichnisses statt des neuesten Releases installieren"
            echo "  -h, --help    Diese Hilfe"
            echo ""
            echo "Ohne Option wird das höchste Release-Tag v<major>.<minor>.<patch> gebaut und installiert."
            echo "Reste einer manuellen Installation entfernt vorher scripts/uninstallLinux.sh --manual-only."
            exit 0 ;;
        *) fail "Unbekannte Option: $1" ;;
    esac
done

[ "$(id -u)" -ne 0 ] || fail "Bitte als normaler Benutzer aufrufen - sudo wird bei Bedarf selbst angefordert."  # the package registers the sudo caller as service user
command -v apt-get &>/dev/null || fail "apt-get fehlt - das Skript installiert ein Debian-Paket (Debian, Ubuntu)."

installed_version()
{
    [ "$(dpkg-query -W -f='${db:Status-Status}' autofilesync 2>/dev/null)" = installed ] || return 0
    dpkg-query -W -f='${Version}' autofilesync
}

latest_tag()
{
    git -C "${SOURCE_DIR}" fetch --tags --quiet origin 2>/dev/null || warn "Tags von origin nicht abrufbar - verwende die lokal bekannten."
    git -C "${SOURCE_DIR}" tag -l 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname | head -n 1
}

check_not_older()
{
    local installed="$1" target="$2"
    [ -n "${installed}" ] || return 0
    dpkg --compare-versions "${target}" ge "${installed}" \
        || fail "Installiert ist ${installed}, neuer als ${target} - kein Downgrade."
}

checkout_release()
{
    info "=== Quellen von $1 bereitstellen ==="
    if [ -d "${RELEASE_SOURCE}" ]; then
        git -C "${RELEASE_SOURCE}" checkout --quiet --detach "$1"
    else
        git -C "${SOURCE_DIR}" worktree add --quiet --detach "${RELEASE_SOURCE}" "$1"
    fi
}

build_package()
{
    rm -rf "${PACKAGE_DIR}"
    "$1/scripts/makeDebianPackage.sh" --output "${PACKAGE_DIR}" >&2  # stdout carries only the package path; the release's own script matches the layout of its sources
    find "${PACKAGE_DIR}" -maxdepth 1 -name 'autofilesync_*.deb' | head -n 1
}

service_enabled()
{
    [ -d /run/systemd/system ] && systemctl -q is-enabled "${UNIT}" 2>/dev/null
}

install_package()
{
    info "=== Paket installieren ==="
    local staging; staging="$(mktemp -d)"
    chmod 755 "${staging}"  # apt downloads as user _apt, which cannot read below the home directory
    install -m 644 "$1" "${staging}/"
    sudo apt-get install -y --reinstall "${staging}/$(basename "$1")" || { rm -rf "${staging}"; fail "Installation fehlgeschlagen."; }
    rm -rf "${staging}"
}

# The package enables the service only on its first installation; after an
# earlier removal the service user is still configured but the unit is disabled.
enable_service()
{
    [ -d /run/systemd/system ] && sudo test -f "${DROPIN}" || return 0
    service_enabled && return 0
    info "=== Hintergrunddienst aktivieren ==="
    sudo systemctl enable --now "${UNIT}"
}

summary()
{
    info "=== Fertig ==="
    echo "Installiert: autofilesync $(installed_version)"
    echo "Programm: $(command -v autofilesync || echo 'nicht im Suchpfad')"
    service_enabled && echo "Hintergrunddienst: $(systemctl is-active "${UNIT}" 2>/dev/null || true)" || echo "Hintergrunddienst: nicht aktiviert"
    pgrep -x autofilesync >/dev/null && warn "AutoFileSync läuft noch in der alten Version - bitte neu starten." || true
}

INSTALLED="$(installed_version)"
if [ "${FROM_HEAD}" = true ]; then
    SOURCE="${SOURCE_DIR}"
    info "=== Aktuellen Stand aus ${SOURCE_DIR} installieren ==="
else
    TAG="$(latest_tag)"
    [ -n "${TAG}" ] || fail "Kein Release-Tag v<major>.<minor>.<patch> gefunden."
    check_not_older "${INSTALLED}" "${TAG#v}"
    [ "${INSTALLED}" != "${TAG#v}" ] || { info "autofilesync ${INSTALLED} ist bereits die neueste Version."; exit 0; }
    info "=== ${TAG} installieren (bisher: ${INSTALLED:-nicht installiert}) ==="
    checkout_release "${TAG}"
    SOURCE="${RELEASE_SOURCE}"
fi
PACKAGE="$(build_package "${SOURCE}")"
[ -f "${PACKAGE}" ] || fail "Kein Paket in ${PACKAGE_DIR} entstanden."
"${SOURCE_DIR}/scripts/uninstallLinux.sh" --manual-only --yes  # leftovers in /usr/local and /etc would shadow the packaged program and unit
install_package "${PACKAGE}"
enable_service
summary
