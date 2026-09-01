#!/bin/bash
sudo systemctl stop backgroundsync.service
set -euo pipefail
#sudo mkdir -p /opt/backgroundsync
#sudo chown welschj:welschj /opt/backgroundsync
sudo cp build/release/backgroundsync /usr/sbin
sudo cp backgroundsync.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now backgroundsync.service
sudo systemctl status backgroundsync.service
# Diagnose
sudo journalctl -u backgroundsync.service -b -n 200 --no-pager

