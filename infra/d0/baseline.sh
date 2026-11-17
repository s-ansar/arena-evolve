#!/usr/bin/env bash
# Phase D0: baseline benchmarks for CPU, memory, disk and network, so later phases can be
# compared against what this machine can actually do.
#   bash infra/d0/baseline.sh [output directory]
# Default output: docs/hardware/raw/. Takes about 4 minutes. Needs sysbench, stress-ng,
# fio, iperf3 (and Docker for the container network test, skipped if missing).
#
# Close other heavy programs first: everything here is sensitive to background load.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="${1:-$repo/docs/hardware/raw}"
mkdir -p "$out"
threads="$(nproc)"

run() {  # run <name> <command...>: print, run, and save the output to <name>.txt
  local name="$1"; shift
  echo ">> $name"
  { echo "\$ $*"; "$@"; } > "$out/$name.txt" 2>&1
}

# ---- CPU ----
# sysbench cpu: counts how many prime-number checks finish per second.
run cpu_sysbench_1thread sysbench cpu --threads=1 --time=15 run
run cpu_sysbench_all     sysbench cpu --threads="$threads" --time=15 run
# stress-ng matrix product: floating-point heavy, closer to the neural-network workload.
run cpu_stressng_matrix stress-ng --matrix "$threads" --timeout 20s --metrics-brief

# ---- Memory ----
# Sequential bandwidth with 1 MiB blocks; reading and writing are measured separately.
for op in read write; do
  run "mem_${op}_1thread" sysbench memory --memory-block-size=1M --memory-total-size=100G \
      --memory-oper="$op" --threads=1 --time=10 run
  run "mem_${op}_all" sysbench memory --memory-block-size=1M --memory-total-size=100G \
      --memory-oper="$op" --threads="$threads" --time=10 run
done

# ---- Disk ----
# fio with direct=1 bypasses the page cache, so it measures the device, not RAM.
# Linux file system (ext4 inside WSL's virtual disk, stored on D: on the NVMe SSD):
work="$HOME/fio-d0"
mkdir -p "$work"
fio_job() {  # fio_job <name> <dir> <rw> <block size> <queue depth> <size>
  run "$1" fio --name="$1" --directory="$2" --rw="$3" --bs="$4" --iodepth="$5" --size="$6" \
      --ioengine=libaio --direct=1 --runtime=20 --time_based --group_reporting
}
fio_job disk_seq_read   "$work" read      1M 32 1G
fio_job disk_seq_write  "$work" write     1M 32 1G
fio_job disk_rand_read  "$work" randread  4k 32 1G
fio_job disk_rand_write "$work" randwrite 4k 32 1G
rm -rf "$work"

# Windows file system seen from Linux (/mnt/c goes through the 9p file-sharing protocol).
# Same device underneath, so any difference is the cost of crossing between the two OSes.
winwork="/mnt/c/Users/$(whoami)/fio-d0-tmp"
if [[ -d "/mnt/c/Users/$(whoami)" ]]; then
  mkdir -p "$winwork"
  run disk_mntc_seq_read fio --name=mntc --directory="$winwork" --rw=read --bs=1M --size=512M \
      --ioengine=psync --runtime=20 --time_based
  run disk_mntc_rand_read fio --name=mntc4k --directory="$winwork" --rw=randread --bs=4k --size=256M \
      --ioengine=psync --runtime=20 --time_based
  rm -rf "$winwork"
fi

# ---- Network ----
# Loopback TCP: no wire at all, so this is the ceiling of the Linux network stack here.
iperf3 --server --one-off --daemon --port 5201
sleep 0.5
run net_loopback_tcp iperf3 --client 127.0.0.1 --port 5201 --time 10
# One client to a container over Docker's default bridge network (a virtual Ethernet pair),
# the path containers will use in the platform phases.
if command -v docker > /dev/null; then
  iperf3 --server --one-off --daemon --port 5202
  sleep 0.5
  gateway="$(docker network inspect bridge --format '{{(index .IPAM.Config 0).Gateway}}')"
  run net_docker_bridge_tcp docker run --rm ubuntu:24.04 bash -c \
      "apt-get update -qq > /dev/null && apt-get install -y -qq iperf3 > /dev/null 2>&1 && iperf3 --client $gateway --port 5202 --time 10"
fi

echo "results in $out"
