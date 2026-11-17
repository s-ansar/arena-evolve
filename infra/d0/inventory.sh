#!/usr/bin/env bash
# Phase D0: record what hardware Linux (WSL2) can see. Read-only, needs no root.
#   bash infra/d0/inventory.sh [output file]
# Default output: docs/hardware/raw/inventory.txt
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="${1:-$repo/docs/hardware/raw/inventory.txt}"
mkdir -p "$(dirname "$out")"

section() { printf '\n===== %s =====\n' "$1"; }

{
  echo "Inventory taken $(date -u +%Y-%m-%dT%H:%MZ) on $(uname -r)"

  section "CPU (lscpu)"
  lscpu | grep -vE '^(Flags|Vulnerability)'
  section "Selected CPU features"
  for flag in avx2 avx512f aes sha_ni; do
    if grep -qw "$flag" /proc/cpuinfo; then echo "$flag: yes"; else echo "$flag: no"; fi
  done

  section "Memory (free)"
  free -h
  section "NUMA (numactl --hardware)"
  numactl --hardware

  section "Topology (lstopo)"
  lstopo-no-graphics --no-io

  section "Block devices (lsblk)"
  lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS
  section "File systems (df)"
  df -hT / /mnt/c 2>/dev/null || df -hT /

  section "PCI devices (lspci)"
  lspci

  section "GPU (nvidia-smi through WSL)"
  if [[ -x /usr/lib/wsl/lib/nvidia-smi ]]; then
    /usr/lib/wsl/lib/nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv
  else
    echo "nvidia-smi not available"
  fi

  section "Network interfaces (ip -brief addr)"
  ip -brief addr
} > "$out"

echo "wrote $out"
