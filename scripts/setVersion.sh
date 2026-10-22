#!/bin/bash
# Sets the program version everywhere the sources carry it: AFS_Desktop/include/Version.h
# (the single source for the installers, the resource script and the About box),
# Version.txt and the heading of the newest section in doc/CHANGELOG.md.
# Usage: scripts/setVersion.sh <major.minor.patch> [--date YYYY-MM-DD | --wip]
set -e
SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
HEADER="${SOURCE_DIR}/AFS_Desktop/include/Version.h"
VERSION_TXT="${SOURCE_DIR}/Version.txt"
CHANGELOG="${SOURCE_DIR}/doc/CHANGELOG.md"
WIP_TEXT="in Arbeit"
DATE="$(date +%F)"
WIP=false
VERSION=""
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

info() { echo -e "${GREEN}$*${NC}"; }
warn() { echo -e "${YELLOW}$*${NC}" >&2; }
fail() { echo -e "${RED}$*${NC}" >&2; exit 1; }

usage()
{
    echo "Aufruf: $0 <major.minor.patch> [OPTIONEN]"
    echo "  --date DATUM    Datum der Veröffentlichung im Changelog (Vorgabe: heute)"
    echo "  --wip           Neue Version als „${WIP_TEXT}“ im Changelog anlegen statt zu veröffentlichen"
    echo "  -h, --help      Diese Hilfe"
    echo ""
    echo "Setzt die Version in AFS_Desktop/include/Version.h, Version.txt und doc/CHANGELOG.md."
    echo "Nicht berührt werden die Android-App (AFS_Android/app/build.gradle.kts) und die Website."
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --date) DATE="$2"; shift 2 ;;
        --wip) WIP=true; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) fail "Unbekannte Option: $1" ;;
        *) [ -z "${VERSION}" ] || fail "Nur eine Version angeben."; VERSION="$1"; shift ;;
    esac
done

[ -n "${VERSION}" ] || { usage >&2; exit 1; }
[[ "${VERSION}" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] || fail "Ungültige Version „${VERSION}“: erwartet wird major.minor.patch, z.B. 0.4.0."
MAJOR="${BASH_REMATCH[1]}"; MINOR="${BASH_REMATCH[2]}"; PATCH="${BASH_REMATCH[3]}"
[[ "${DATE}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] || fail "Ungültiges Datum „${DATE}“: erwartet wird JJJJ-MM-TT."
for file in "${HEADER}" "${VERSION_TXT}" "${CHANGELOG}"; do
    [ -f "${file}" ] || fail "Datei fehlt: ${file}"
done

current_version() { sed -n 's/^#define afsVERSION_TEXT "\(.*\)".*/\1/p' "${HEADER}"; }

set_header()
{
    sed -i -E \
        -e "s/^(#define afsVERSION_MAJOR )[0-9]+/\1${MAJOR}/" \
        -e "s/^(#define afsVERSION_MINOR )[0-9]+/\1${MINOR}/" \
        -e "s/^(#define afsVERSION_PATCH )[0-9]+/\1${PATCH}/" \
        -e "s/^(#define afsVERSION_TEXT \")[^\"]*\"/\1${VERSION}\"/" "${HEADER}"
    [ "$(current_version)" = "${VERSION}" ] || fail "Version.h konnte nicht geschrieben werden."
}

set_version_txt()
{
    sed -i -E "1s/^Aktuell ist Version .*/Aktuell ist Version ${VERSION}/" "${VERSION_TXT}"
    grep -q "^Aktuell ist Version ${VERSION}$" "${VERSION_TXT}" || fail "Version.txt beginnt nicht mit „Aktuell ist Version …“."
}

newest_heading() { grep -m1 -n '^## ' "${CHANGELOG}"; }

set_changelog()  # the newest section names the version: renamed when it is still work in progress, inserted otherwise
{
    local heading suffix line
    if ${WIP}; then suffix="${WIP_TEXT}"; else suffix="${DATE}"; fi
    heading="## ${VERSION} – ${suffix}"
    line="$(newest_heading | cut -d: -f1)"
    [ -n "${line}" ] || fail "doc/CHANGELOG.md enthält keinen Abschnitt „## …“."
    if sed -n "${line}p" "${CHANGELOG}" | grep -qE "^## (${VERSION} – |.* – ${WIP_TEXT}$)"; then
        sed -i "${line}s/.*/${heading}/" "${CHANGELOG}"
    else
        sed -i "${line}i\\${heading}\\n" "${CHANGELOG}"
        warn "Neuer Abschnitt „${heading}“ in doc/CHANGELOG.md angelegt; die Einträge fehlen noch."
    fi
}

OLD="$(current_version)"
set_header
set_version_txt
set_changelog
info "Version ${OLD} → ${VERSION} gesetzt:"
git -C "${SOURCE_DIR}" --no-pager diff --stat -- "${HEADER}" "${VERSION_TXT}" "${CHANGELOG}" || true
${WIP} || info "Zum Veröffentlichen: git commit -am \"Release ${VERSION}\" && git tag v${VERSION}"
