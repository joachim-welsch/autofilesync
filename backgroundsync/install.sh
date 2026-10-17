#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DAEMON="${SCRIPT_DIR}/../build/Release/backgroundsync"
LOCALE_DIR="${SCRIPT_DIR}/../build/Release/locale"
[ -f "${DAEMON}" ] || { echo "${DAEMON} fehlt - zuerst scripts/build.sh -r im Projektverzeichnis aufrufen."; exit 1; }
[ -d "${LOCALE_DIR}" ] || { echo "${LOCALE_DIR} fehlt - gettext (msgfmt) installieren und scripts/build.sh -r erneut aufrufen."; exit 1; }
sudo systemctl stop backgroundsync.service 2>/dev/null || true
sudo install -m 755 "${DAEMON}" /usr/sbin/backgroundsync
for MO in "${LOCALE_DIR}"/*/LC_MESSAGES/autofilesync.mo; do
    sudo install -D -m 644 "${MO}" "/usr/share/locale/${MO#"${LOCALE_DIR}"/}"
done
sudo cp "${SCRIPT_DIR}/backgroundsync.service" /etc/systemd/system/
sudo tee /etc/polkit-1/rules.d/50-backgroundsync.rules > /dev/null <<EOF
// the user who installed the service may start and stop it without a password
polkit.addRule(function(action, subject) {
    if (action.id == "org.freedesktop.systemd1.manage-units" &&
        action.lookup("unit") == "backgroundsync.service" &&
        subject.user == "$(id -un)" &&
        ["start", "stop", "restart"].indexOf(action.lookup("verb")) >= 0)
        return polkit.Result.YES;
});
EOF
sudo systemctl daemon-reload
sudo systemctl enable --now backgroundsync.service
sudo systemctl status backgroundsync.service --no-pager || true
# Diagnose
sudo journalctl -u backgroundsync.service -b -n 200 --no-pager
