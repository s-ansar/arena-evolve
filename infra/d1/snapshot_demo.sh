#!/usr/bin/env bash
# Phase D1: snapshots. Save the provisioned server, break it on purpose, restore, check.
#   bash infra/d1/snapshot_demo.sh
set -euo pipefail
cd "$HOME/arena-d1/repo/infra/d1"
export VAGRANT_DEFAULT_PROVIDER=libvirt
quiet() { grep -vF '[fog]' || true; }

echo "== save snapshot 'provisioned'"
vagrant snapshot save server provisioned 2>&1 | quiet
vagrant snapshot list server 2>&1 | quiet

echo "== break the server: delete the binary and stop the service"
vagrant ssh server -- -q 'sudo rm /opt/arena/bin/arena_server; sudo systemctl stop arena-server; systemctl is-active arena-server || true' 2>&1 | quiet

echo "== restore the snapshot"
start=$(date +%s)
vagrant snapshot restore server provisioned --no-provision 2>&1 | quiet
echo "restore took $(( $(date +%s) - start )) s"

echo "== after restore"
vagrant ssh server -- -q 'ls -l /opt/arena/bin/arena_server; systemctl is-active arena-server' 2>&1 | quiet
vagrant snapshot delete server provisioned 2>&1 | quiet
