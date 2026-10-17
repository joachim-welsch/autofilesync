#!/bin/bash
# Builds the Debian package autofilesync_<version>_<architecture>.deb with the GUI,
# the backgroundsync service, the message catalogues, help book, menu entry and icons.
set -e
umask 022  # the package must not inherit group-writable files from a developer umask

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
DEBIAN_DIR="${SOURCE_DIR}/AFS_Desktop/installer/debian"
BUILD_DIR="${SOURCE_DIR}/build/Debian"
STAGING="${BUILD_DIR}/debian/autofilesync"  # the layout dpkg-shlibdeps expects
OUTPUT_DIR="${SOURCE_DIR}/build"
SKIP_BUILD=false
RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'

info() { echo -e "${GREEN}$*${NC}"; }
fail() { echo -e "${RED}$*${NC}" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) SKIP_BUILD=true; shift ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Aufruf: $0 [OPTIONEN]"
            echo "  --no-build      Vorhandenen Build in build/Debian verwenden"
            echo "  --output DIR    Ablageort des Pakets (Vorgabe: build/)"
            echo "  -h, --help      Diese Hilfe"
            echo ""
            echo "Umgebung:"
            echo "  AFS_MAINTAINER  Maintainer-Feld (Vorgabe: git user.name <user.email>)"
            exit 0 ;;
        *) fail "Unbekannte Option: $1" ;;
    esac
done

version() { sed -n 's/^#define afsVERSION_TEXT "\(.*\)".*/\1/p' "${SOURCE_DIR}/AFS_Desktop/include/Version.h"; }

maintainer()
{
    [ -n "${AFS_MAINTAINER}" ] && { echo "${AFS_MAINTAINER}"; return; }
    echo "$(git -C "${SOURCE_DIR}" config user.name) <$(git -C "${SOURCE_DIR}" config user.email)>"
}

check_dependencies()
{
    info "=== Abhängigkeiten prüfen ==="
    local missing=() cmd
    for cmd in cmake msgfmt dpkg-deb dpkg-shlibdeps dpkg-architecture identify convert strip git; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done
    [ ${#missing[@]} -eq 0 ] || fail "Fehlt: ${missing[*]} (sudo apt install cmake gettext dpkg-dev imagemagick binutils git)"
    [ -x "$(distribution_wx_config)" ] || fail "wxWidgets der Distribution fehlt: sudo apt install libwxgtk3.2-dev"  # a self-built wxWidgets under /usr/local does not exist on the target systems
}

distribution_library_dir()
{
    echo "/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
}

distribution_wx_config()
{
    echo "$(distribution_library_dir)/wx/config/gtk3-unicode-3.2"
}

existing_llama_checkout()
{
    local dir
    for dir in "${BUILD_DIR}/_deps" "${SOURCE_DIR}/build/Release/_deps" "${SOURCE_DIR}/build/_deps"; do
        [ -f "${dir}/llama_cpp-src/CMakeLists.txt" ] && { echo "${dir}/llama_cpp-src"; return; }
    done
}

build_release()
{
    info "=== Release bauen ==="
    local llama; llama="$(existing_llama_checkout)"
    cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
        -DwxWidgets_CONFIG_EXECUTABLE="$(distribution_wx_config)" \
        -DGGML_NATIVE=OFF ${llama:+-DFETCHCONTENT_SOURCE_DIR_LLAMA_CPP="${llama}"}  # the package runs on other CPUs than the build machine
    cmake --build "${BUILD_DIR}" --config Release --target autofilesync backgroundsync translations -j "$(nproc)"
}

verify_build()
{
    local file
    for file in autofilesync backgroundsync; do
        [ -x "${BUILD_DIR}/${file}" ] || fail "${BUILD_DIR}/${file} fehlt - ohne --no-build aufrufen."
    done
    for file in "${SOURCE_DIR}"/po/*.po; do
        local language; language="$(basename "${file}" .po)"
        [ -f "${BUILD_DIR}/locale/${language}/LC_MESSAGES/autofilesync.mo" ] || fail "Übersetzung ${language} fehlt im Build."
    done
    [ -f "${SOURCE_DIR}/AFS_Desktop/help/help.htb" ] || fail "AFS_Desktop/help/help.htb fehlt."
    for project in "${SOURCE_DIR}"/AFS_Desktop/help/*/help.hhp; do
        [ -f "${project%/help.hhp}/help.htb" ] || fail "${project%/help.hhp}/help.htb fehlt."
    done
}

stage_programs()
{
    install -D -m 755 "${BUILD_DIR}/autofilesync" "${STAGING}/usr/lib/autofilesync/autofilesync"
    install -D -m 755 "${BUILD_DIR}/backgroundsync" "${STAGING}/usr/sbin/backgroundsync"
    install -D -m 755 "${DEBIAN_DIR}/wait-network-mounts" "${STAGING}/usr/lib/autofilesync/wait-network-mounts"
    strip --strip-unneeded "${STAGING}/usr/lib/autofilesync/autofilesync" "${STAGING}/usr/sbin/backgroundsync"
    mkdir -p "${STAGING}/usr/bin"
    ln -s ../lib/autofilesync/autofilesync "${STAGING}/usr/bin/autofilesync"  # the program finds help and catalogues beside its resolved /proc/self/exe
}

stage_translations()
{
    local mo
    for mo in "${BUILD_DIR}"/locale/*/LC_MESSAGES/autofilesync.mo; do
        install -D -m 644 "${mo}" "${STAGING}/usr/share/locale/${mo#"${BUILD_DIR}"/locale/}"
    done
    ln -s ../../share/locale "${STAGING}/usr/lib/autofilesync/locale"
}

stage_help()
{
    mkdir -p "${STAGING}/usr/share/autofilesync/help"
    install -m 644 "${SOURCE_DIR}"/AFS_Desktop/help/*.* "${STAGING}/usr/share/autofilesync/help/"
    for project in "${SOURCE_DIR}"/AFS_Desktop/help/*/help.hhp; do
        local language_dir="${project%/help.hhp}"
        mkdir -p "${STAGING}/usr/share/autofilesync/help/${language_dir##*/}"
        install -m 644 "${language_dir}"/* "${STAGING}/usr/share/autofilesync/help/${language_dir##*/}/"
    done
    ln -s ../../share/autofilesync/help "${STAGING}/usr/lib/autofilesync/help"
}

# Debian policy asks for copyright and a compressed changelog under
# /usr/share/doc; the licence text and the licences of the third-party
# components go there too.
stage_documentation()
{
    local doc="${STAGING}/usr/share/doc/autofilesync"
    mkdir -p "${doc}/third-party"
    install -m 644 "${DEBIAN_DIR}/copyright" "${doc}/copyright"
    install -m 644 "${SOURCE_DIR}"/doc/{LICENSING.md,THIRD-PARTY-NOTICES.md} "${doc}/"
    install -m 644 "${SOURCE_DIR}"/third-party/* "${doc}/third-party/"
    {
        printf 'autofilesync (%s) unstable; urgency=medium\n\n  * Siehe CHANGELOG.md.\n\n -- %s  %s\n' \
            "${VERSION}" "${MAINTAINER}" "$(date -R)"
    } | gzip -9n > "${doc}/changelog.Debian.gz"
    install -m 644 "${SOURCE_DIR}/doc/CHANGELOG.md" "${doc}/changelog"
    gzip -9n "${doc}/changelog"
}

largest_icon_frame()
{
    identify -format '%w %p\n' "$1" | sort -n | tail -1 | cut -d' ' -f2
}

stage_desktop_integration()
{
    local ico="${SOURCE_DIR}/AFS_Desktop/icons/afs.ico" frame size
    frame="$(largest_icon_frame "${ico}")"
    for size in 16 24 32 48 64 128 256; do
        local icon_dir="${STAGING}/usr/share/icons/hicolor/${size}x${size}/apps"
        mkdir -p "${icon_dir}"
        convert "${ico}[${frame}]" -resize "${size}x${size}" "${icon_dir}/autofilesync.png"
    done
    install -D -m 644 "${DEBIAN_DIR}/autofilesync.desktop" "${STAGING}/usr/share/applications/autofilesync.desktop"
    install -D -m 644 "${DEBIAN_DIR}/backgroundsync.service" "${STAGING}/usr/lib/systemd/system/backgroundsync.service"
}

shared_library_depends()
{
    printf 'Source: autofilesync\n\nPackage: autofilesync\nArchitecture: any\n' > "${BUILD_DIR}/debian/control"  # dpkg-shlibdeps insists on a source package around the binaries
    (cd "${BUILD_DIR}" && dpkg-shlibdeps -O -l"$(distribution_library_dir)" -e"${STAGING}/usr/lib/autofilesync/autofilesync" -e"${STAGING}/usr/sbin/backgroundsync") \
        | sed -n 's/^shlibs:Depends=//p'  # -l: ld.so.conf searches /usr/local/lib first, where a self-built wxWidgets hides the packaged one
}

write_control()
{
    local depends; depends="$(shared_library_depends)"
    [ -n "${depends}" ] || fail "Paketabhängigkeiten nicht ermittelbar (dpkg-shlibdeps) - jede gelinkte Bibliothek muss aus einem Paket stammen."
    mkdir -p "${STAGING}/DEBIAN"
    sed -e "s|@VERSION@|$(version)|" \
        -e "s|@ARCHITECTURE@|$(dpkg-architecture -qDEB_HOST_ARCH)|" \
        -e "s|@MAINTAINER@|$(maintainer)|" \
        -e "s|@INSTALLED_SIZE@|$(du -sk --exclude=DEBIAN "${STAGING}" | cut -f1)|" \
        -e "s|@DEPENDS@|${depends}, systemd|" \
        "${DEBIAN_DIR}/control.in" > "${STAGING}/DEBIAN/control"
    install -m 755 "${DEBIAN_DIR}/postinst" "${DEBIAN_DIR}/prerm" "${DEBIAN_DIR}/postrm" "${STAGING}/DEBIAN/"
    (cd "${STAGING}" && find usr -type f -exec md5sum {} + | sort -k2) > "${STAGING}/DEBIAN/md5sums"
}

readable_by_apt()
{
    local path; path="$(realpath "$1")"
    [ -n "$(find "${path}" -maxdepth 0 -perm -o+r)" ] || return 1
    while [ "${path}" != / ]; do
        path="$(dirname "${path}")"
        [ -n "$(find "${path}" -maxdepth 0 -perm -o+x)" ] || return 1  # apt downloads as user _apt, which needs traversal rights on every parent
    done
}

install_hint()
{
    if readable_by_apt "$1"; then
        echo "Installieren: sudo apt install $(realpath "$1")"
    else
        echo "Installieren: install -m 644 $(realpath "$1") /tmp/ && sudo apt install /tmp/$(basename "$1")"
    fi
}

[ -n "$(version)" ] || fail "Version aus AFS_Desktop/include/Version.h nicht lesbar."
check_dependencies
[ "${SKIP_BUILD}" = true ] || build_release
verify_build

info "=== Paketinhalt zusammenstellen ==="
rm -rf "${STAGING}"
stage_programs
stage_translations
stage_help
stage_documentation
stage_desktop_integration
write_control

info "=== Paket bauen ==="
mkdir -p "${OUTPUT_DIR}"
PACKAGE="${OUTPUT_DIR}/autofilesync_$(version)_$(dpkg-architecture -qDEB_HOST_ARCH).deb"
dpkg-deb --root-owner-group -Zxz --build "${STAGING}" "${PACKAGE}" >/dev/null

info "=== Fertig ==="
echo "Paket: ${PACKAGE} ($(du -h "${PACKAGE}" | cut -f1))"
install_hint "${PACKAGE}"
