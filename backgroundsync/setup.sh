#!/bin/bash
# Creates the system account for the daemon and lets it read the files of the
# user whose configuration it synchronises: setup.sh <user>
user="${1:?Aufruf: setup.sh <Benutzer, dessen Dateien synchronisiert werden>}"
sudo groupadd --system backgroundsync || true
sudo useradd --system --no-create-home --shell /usr/sbin/nologin -g backgroundsync backgroundsync || true
sudo usermod -aG "${user}" backgroundsync
