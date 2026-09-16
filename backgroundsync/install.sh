#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DAEMON="${SCRIPT_DIR}/../build/Release/backgroundsync"
[ -f "${DAEMON}" ] || { echo "${DAEMON} fehlt - zuerst ./build.sh -r im Projektverzeichnis aufrufen."; exit 1; }
sudo systemctl stop backgroundsync.service 2>/dev/null || true
sudo install -m 755 "${DAEMON}" /usr/sbin/backgroundsync
sudo cp "${SCRIPT_DIR}/backgroundsync.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now backgroundsync.service
sudo systemctl status backgroundsync.service --no-pager || true
# Diagnose
sudo journalctl -u backgroundsync.service -b -n 200 --no-pager
