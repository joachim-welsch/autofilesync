#!/bin/bash
# Shared steps that turn the CMake build output into a self-contained
# AutoFileSync.app. Sourced by installMacOS.sh and makeMacOSInstaller.sh.

# Repository root (CMake source directory) and the AFS_Desktop directory that
# holds the resources going into the bundle.
AFS_SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"
AFS_DESKTOP_DIR="${AFS_SOURCE_DIR}/AFS_Desktop"
AFS_RED='\033[0;31m'; AFS_GREEN='\033[0;32m'; AFS_YELLOW='\033[1;33m'; AFS_NC='\033[0m'

afs_info()  { echo -e "${AFS_GREEN}$*${AFS_NC}"; }
afs_warn()  { echo -e "${AFS_YELLOW}$*${AFS_NC}"; }
afs_fail()  { echo -e "${AFS_RED}$*${AFS_NC}" >&2; exit 1; }

afs_version() { sed -n 's/^#define afsVERSION_TEXT "\(.*\)".*/\1/p' "${AFS_DESKTOP_DIR}/include/Version.h"; }

# Version.h is the single source of truth; the checked-in Info.plist carries a
# placeholder so the version never has to be maintained in two places.
afs_stamp_version() {
    local plist="$1/Contents/Info.plist" version; version="$(afs_version)"
    [ -n "$version" ] || afs_fail "Version aus AFS_Desktop/include/Version.h nicht lesbar."
    local key
    for key in CFBundleShortVersionString CFBundleVersion; do
        /usr/libexec/PlistBuddy -c "Set :${key} ${version}" "$plist" >/dev/null
    done
}

afs_check_dependencies() {
    afs_info "=== Abhängigkeiten prüfen ==="
    local missing=()
    for cmd in cmake pkg-config install_name_tool codesign hdiutil; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done
    local formula
    for formula in libssh boost openssl@3; do
        brew list "$formula" &>/dev/null || missing+=("brew install $formula")
    done
    brew list wxwidgets@3.2 &>/dev/null || brew list wxwidgets &>/dev/null \
        || missing+=("brew install wxwidgets@3.2")
    [ ${#missing[@]} -eq 0 ] || afs_fail "Fehlt: ${missing[*]}"
    afs_info "Alle Abhängigkeiten vorhanden."
}

afs_build_release() {
    local build_dir="$1"
    afs_info "=== Release bauen ==="
    cmake -S "${AFS_SOURCE_DIR}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build "${build_dir}" --config Release --target autofilesync -j "$(sysctl -n hw.ncpu)"
    [ -x "${build_dir}/autofilesync.app/Contents/MacOS/autofilesync" ] \
        || afs_fail "Build hat kein Programmbündel erzeugt."
}

# The help book is looked up next to the executable and in the bundle's
# resources; only the second works for an app installed under /Applications.
afs_add_resources() {
    local app="$1"
    afs_info "=== Hilfe und Symbole einbetten ==="
    rm -rf "${app}/Contents/Resources/help"
    cp -R "${AFS_DESKTOP_DIR}/help" "${app}/Contents/Resources/help"
    [ -f "${app}/Contents/Resources/help/help.htb" ] || afs_fail "help.htb fehlt in ${app}."
    cp -f "${AFS_DESKTOP_DIR}/icons/afs.icns" "${app}/Contents/Resources/afs.icns"
    afs_add_licenses "${app}"
}

# The licence texts travel inside the bundle: the GPL, its explanation and the
# notices of the third-party libraries embedded as frameworks.
afs_add_licenses() {
    local app="$1" root="${AFS_DESKTOP_DIR}/.." licenses="${app}/Contents/Resources/licenses"
    rm -rf "${licenses}"; mkdir -p "${licenses}"
    cp -f "${root}"/{LICENSE,LICENSING.md,THIRD-PARTY-NOTICES.md,CHANGELOG.md} "${licenses}/"
    cp -R "${root}/third-party" "${licenses}/third-party"
}

# Every non-system library a binary loads, its own install name excluded. The
# already rewritten @executable_path entries stay in: bundling has to be
# repeatable on a bundle whose executable was rewritten by an earlier run.
afs_all_deps() {
    local own_id; own_id="$(otool -D "$1" | tail -n +2)"
    otool -L "$1" | awk 'NR>1 {print $1}' | grep -Ev '^(/usr/lib|/System)' \
        | grep -Fxv "${own_id:-///no-install-name///}" || true
}

afs_external_deps() { afs_all_deps "$1" | grep -Ev '^@executable_path' || true; }

# @rpath, @executable_path and @loader_path entries carry no usable path of
# their own; Homebrew's prefix is where every dependency this project links
# actually lives.
afs_resolve_dep() {
    local dep="$1" name; name="$(basename "$dep")"
    case "$dep" in
        @*) local candidate
            for candidate in "$(brew --prefix)/lib/${name}" "$(brew --prefix)/opt/"*/lib/"${name}"; do
                [ -f "$candidate" ] && { echo "$candidate"; return; }
            done
            return 1 ;;
        *) [ -f "$dep" ] && echo "$dep" || return 1 ;;
    esac
}

afs_bundle_dylibs() {
    local app="$1" frameworks="$1/Contents/Frameworks" executable="$1/Contents/MacOS/autofilesync"
    afs_info "=== Bibliotheken einbetten ==="
    rm -rf "${frameworks}"; mkdir -p "${frameworks}"
    local -a queue=("${executable}")
    while [ ${#queue[@]} -gt 0 ]; do
        local current="${queue[0]}"; queue=("${queue[@]:1}")
        local dep
        for dep in $(afs_all_deps "$current"); do
            local source; source="$(afs_resolve_dep "$dep")" \
                || afs_fail "Bibliothek nicht auffindbar: ${dep} (benötigt von $(basename "$current"))"
            # Homebrew links every library under an unversioned symlink as well.
            # Copying it under the name a reference happens to use would embed the
            # same code twice, and two copies of the same Objective-C classes make
            # the runtime pick one of them at random.
            local name; name="$(basename "$(readlink -f "$source")")"
            if [ ! -f "${frameworks}/${name}" ]; then
                echo "  ${name}"
                cp -f "$source" "${frameworks}/${name}"
                chmod 755 "${frameworks}/${name}"
                install_name_tool -id "@executable_path/../Frameworks/${name}" "${frameworks}/${name}" 2>/dev/null
                queue+=("${frameworks}/${name}")
            fi
            install_name_tool -change "$dep" "@executable_path/../Frameworks/${name}" "$current" 2>/dev/null
        done
    done
}

# Signatures cover the whole bundle, so nothing may be added to it afterwards.
afs_codesign_app() {
    local app="$1" identity="${AFS_CODESIGN_IDENTITY:--}"
    afs_info "=== Signieren (${identity}) ==="
    find "${app}/Contents/Frameworks" -name '*.dylib' -exec codesign --force --timestamp=none --sign "$identity" {} \;
    codesign --force --timestamp=none --sign "$identity" "${app}"
}

afs_verify_bundle() {
    local app="$1" executable="$1/Contents/MacOS/autofilesync" failures=0
    afs_info "=== Bündel prüfen ==="
    local -a libraries
    shopt -s nullglob; libraries=("${app}"/Contents/Frameworks/*.dylib); shopt -u nullglob
    [ ${#libraries[@]} -gt 0 ] || { afs_warn "  Keine Bibliothek eingebettet"; failures=1; }
    local binary
    for binary in "$executable" "${libraries[@]}"; do
        local leaked; leaked="$(afs_external_deps "$binary")"
        [ -z "$leaked" ] || { afs_warn "  $(basename "$binary") verweist nach außen: ${leaked//$'\n'/ }"; failures=1; }
        local dep
        for dep in $(afs_all_deps "$binary" | grep '^@executable_path' || true); do
            [ -f "${app}/Contents/MacOS/${dep#@executable_path/}" ] \
                || { afs_warn "  $(basename "$binary") braucht ${dep##*/}, das im Bündel fehlt"; failures=1; }
        done
    done
    local duplicates; duplicates="$(shasum "${libraries[@]}" 2>/dev/null | awk '{print $1}' | sort | uniq -d)"
    [ -z "$duplicates" ] || { afs_warn "  Bibliothek doppelt eingebettet"; failures=1; }
    [ -f "${app}/Contents/Resources/help/help.htb" ] || { afs_warn "  help.htb fehlt"; failures=1; }
    [ -f "${app}/Contents/Resources/afs.icns" ] || { afs_warn "  afs.icns fehlt"; failures=1; }
    [ -f "${app}/Contents/Resources/licenses/LICENSE" ] || { afs_warn "  Lizenztexte fehlen"; failures=1; }
    codesign --verify --deep --strict "${app}" 2>/dev/null || { afs_warn "  Signatur ungültig"; failures=1; }
    [ $failures -eq 0 ] || afs_fail "Bündel unvollständig."
    afs_info "Bündel ist eigenständig (${#libraries[@]} Bibliotheken)."
}

# One call for the whole path from build output to a signed, verified bundle.
afs_make_bundle() {
    local build_dir="$1" app="$1/autofilesync.app"
    afs_check_dependencies
    afs_build_release "${build_dir}"
    afs_add_resources "${app}"
    afs_stamp_version "${app}"
    afs_bundle_dylibs "${app}"
    afs_codesign_app "${app}"
    afs_verify_bundle "${app}"
}
