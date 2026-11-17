#!/usr/bin/env bash
# Runs as root on the server VM after common.sh: install arena_server as a systemd service
# and open its ports, but only to the private lab network.
set -euo pipefail

# A system account with no home directory and no login shell, so the service has no
# rights beyond what it needs.
id arena > /dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin arena

install -m 0644 /vagrant/infra/d1/arena-server.service /etc/systemd/system/arena-server.service
systemctl daemon-reload
systemctl enable --now arena-server
systemctl restart arena-server  # pick up a new binary when re-provisioning

# Game and monitoring ports: from the 192.168.56.0/24 lab network only.
ufw allow from 192.168.56.0/24 to any port 5555 proto tcp comment 'arena controllers'
ufw allow from 192.168.56.0/24 to any port 5555 proto udp comment 'arena watchers'
ufw allow from 192.168.56.0/24 to any port 8080 proto tcp comment 'arena metrics'
ufw status numbered

systemctl --no-pager status arena-server | head -5
