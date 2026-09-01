#!/bin/bash
sudo groupadd --system backgroundsync || true
sudo useradd --system --no-create-home --shell /usr/sbin/nologin -g backgroundsync backgroundsync || true

sudo usermod -aG welschj  backgroundsync
