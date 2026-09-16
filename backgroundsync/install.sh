#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DAEMON="${SCRIPT_DIR}/../build/Release/backgroundsync"
LOCALE_DIR="${SCRIPT_DIR}/../build/Release/locale"
[ -f "${DAEMON}" ] || { echo "${DAEMON} fehlt - zuerst ./build.sh -r im Projektverzeichnis aufrufen."; exit 1; }
[ -d "${LOCALE_DIR}" ] || { echo "${LOCALE_DIR} fehlt - gettext (msgfmt) installieren und ./build.sh -r erneut aufrufen."; exit 1; }
sudo systemctl stop backgroundsync.service 2>/dev/null || true
sudo install -m 755 "${DAEMON}" /usr/sbin/backgroundsync
for MO in "${LOCALE_DIR}"/*/LC_MESSAGES/autofilesync.mo; do
    sudo install -D -m 644 "${MO}" "/usr/share/locale/${MO#"${LOCALE_DIR}"/}"
done
sudo cp "${SCRIPT_DIR}/backgroundsync.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now backgroundsync.service
sudo systemctl status backgroundsync.service --no-pager || true
# Diagnose
sudo journalctl -u backgroundsync.service -b -n 200 --no-pager
