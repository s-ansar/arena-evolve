#!/usr/bin/env bash
# Bring the D1 VMs up from WSL. Vagrant must run from a Linux path (not /mnt/c), because
# libvirt and rsync need real Linux file permissions, so this copies infra/d1 to ~/arena-d1
# and runs Vagrant there with the repository as the synced folder.
#   bash infra/d1/up.sh             vagrant up (creates the VMs the first time)
#   bash infra/d1/up.sh destroy -f  any other vagrant command, run in the same place
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$HOME/arena-d1"

# A Linux-side copy of the repository for the rsync synced folder.
mkdir -p "$work/repo"
rsync -a --delete --exclude .git --exclude 'build*' --exclude docs/hardware --exclude docs/captures \
      --exclude docs/images --exclude infra/d1/.vagrant "$repo/" "$work/repo/"

cd "$work/repo/infra/d1"
export VAGRANT_DEFAULT_PROVIDER=libvirt
if [[ $# -eq 0 ]]; then
  vagrant up
else
  vagrant "$@"
fi
