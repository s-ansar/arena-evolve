#!/usr/bin/env bash
# Pull the headline number out of each raw D0 result file.
#   bash infra/d0/summarise.sh [raw directory]
set -uo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
raw="${1:-$repo/docs/hardware/raw}"

for f in cpu_sysbench_1thread cpu_sysbench_all; do
  printf '%-24s %s events/s\n' "$f" "$(grep -m1 'events per second' "$raw/$f.txt" | awk '{print $4}')"
done
printf '%-24s %s\n' cpu_stressng_matrix "$(grep -E 'matrix ' "$raw/cpu_stressng_matrix.txt" | tail -1 | sed 's/  */ /g')"

for f in mem_read_1thread mem_read_all mem_write_1thread mem_write_all; do
  printf '%-24s %s\n' "$f" "$(grep -m1 'MiB/sec' "$raw/$f.txt" | sed 's/^ *//')"
done

for f in disk_seq_read disk_seq_write disk_rand_read disk_rand_write disk_mntc_seq_read disk_mntc_rand_read; do
  [[ -f "$raw/$f.txt" ]] || continue
  printf '%-24s %s\n' "$f" "$(grep -m1 -E '^ +(read|write): IOPS' "$raw/$f.txt" | sed 's/^ *//')"
  printf '%-24s %s\n' "" "$(grep -m1 -E '^ +clat \(' "$raw/$f.txt" | sed 's/^ *//')"
done

for f in net_loopback_tcp net_docker_bridge_tcp; do
  [[ -f "$raw/$f.txt" ]] || continue
  printf '%-24s %s\n' "$f" "$(grep -E 'sender$' "$raw/$f.txt" | tail -1 | awk '{print $7, $8, "retransmits", $9}')"
done
