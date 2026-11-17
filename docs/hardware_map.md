# Hardware map (Arena Platform, phase D0)

Measured 2026-11-17. Everything here can be reproduced:

```bash
bash infra/d0/inventory.sh    # what Linux can see       -> docs/hardware/raw/inventory.txt
bash infra/d0/baseline.sh     # CPU, memory, disk, network benchmarks (about 4 minutes)
bash infra/d0/summarise.sh    # headline numbers from the raw files
powershell -File infra\d0\windows_write_test.ps1 -Drive D:   # native Windows write speed
```

The raw outputs are in [`docs/hardware/raw/`](hardware/raw/). Conditions: on mains power, Windows "Balanced" power plan, normal desktop programs open, one run each. Treat single-run numbers as roughly ±10%.

## The machine

| Part | What it is | Seen inside WSL2 |
|---|---|---|
| Laptop | HP Pavilion Laptop 15-eg0xxx | |
| CPU | Intel Core i5-1135G7 (Tiger Lake): 4 cores / 8 threads, 2.4 GHz base, up to 4.2 GHz turbo, 8 MB L3, AVX-512 | 4 vCPUs, shown as 2 cores × 2 threads; AVX-512 visible |
| Memory | 2 × 4 GB DDR4-3200 (Micron), dual channel | 3.8 GiB, capped by `.wslconfig`; 1 NUMA node |
| Storage | Intel SSD 660p 512 GB, NVMe PCIe 3.0 ×4, QLC flash. Split into C: (155 GB, 27 GB free) and D: (321 GB, 317 GB free) | ext4 on a virtual disk (`/dev/sdd`, 1 TB maximum) stored on D: |
| GPU | NVIDIA GeForce MX350, 2 GB, plus Intel Iris Xe | MX350 visible through `nvidia-smi` (driver 581.83). No PCI passthrough: `lspci` shows only virtual devices |
| Network | Realtek RTL8822CE Wi-Fi 5 (802.11ac 2×2), link at 360 Mbit/s when measured | One virtual NIC behind WSL's network translation |

Inside WSL every device is virtual (`lspci` lists Hyper-V virtio devices and a "Basic Render Driver"), so the hardware facts above come from Windows, and the measurements below are what Linux programs actually get.

## Spec vs measured

### CPU

| Test | Result |
|---|---|
| `sysbench cpu`, 1 thread | 1873 events/s |
| `sysbench cpu`, 4 threads | 6908 events/s, **3.69×** the single-thread result |
| `stress-ng --matrix 4` (floating-point matrix products) | 8414 bogo ops/s |

A 3.69× speed-up on 4 vCPUs is close to linear. If two of the vCPUs were hyper-threads on the same physical core, they would share one set of execution units and the speed-up would be nearer 2.5×. Hyper-V is spreading the 4 vCPUs over separate physical cores. This matters for the Slurm scaling study (D4): with 4 workers, each gets roughly a whole core.

### Memory

| Test | Measured | Theoretical | Share |
|---|---|---|---|
| Sequential read, 4 threads | 43 284 MiB/s | 48 828 MiB/s (2 channels × 8 bytes × 3200 MT/s) | 89% |
| Sequential read, 1 thread | 20 763 MiB/s | | 43% |
| Sequential write, 4 threads | 23 397 MiB/s | | 48% |
| Sequential write, 1 thread | 17 202 MiB/s | | 35% |

- One core can't saturate both memory channels: it runs out of outstanding cache-line requests first. It takes all four to reach 89% of the theoretical figure.
- Writes reach about half the read bandwidth because writing a cache line usually means reading it in first ("read for ownership"), so each written byte crosses the memory bus twice.

### Disk

| Test (fio, direct I/O, queue depth 32) | Measured | Drive rating (Intel 660p 512 GB) |
|---|---|---|
| Sequential read, 1 MiB blocks | 1052 MB/s | 1500 MB/s |
| Random read, 4 KiB blocks | 25 500 IOPS (99.7 MiB/s) | 90 000 IOPS |
| Sequential write, 1 MiB blocks | **66 MB/s** (repeated: 73 MB/s; buffered with fsync: 67 MB/s) | 1000 MB/s |
| Random write, 4 KiB blocks | **560 IOPS** | 220 000 IOPS |

Reads are in the right range. Writes are 15× below the rating, and every repeat agreed. To find out whether WSL or the drive was to blame, the same sequential write was timed natively in Windows with write-through:

| Where | Sequential write |
|---|---|
| WSL ext4 virtual disk (stored on D:) | 66–73 MB/s |
| Windows, D: directly | 82 MiB/s |
| Windows, C: | 123 MiB/s |

So most of the slowness is the SSD itself, not WSL. WSL's virtual disk costs roughly another 20% on top. The likely reason: the 660p uses QLC flash (4 bits per cell). It is fast only while writes land in its SLC cache, and once data has to be committed to QLC cells (forced here by direct or write-through I/O), write speed falls to around 100 MB/s. Reviews of this drive report the same. I can't prove the cause from these tests alone. A next experiment would be to compare small bursts (cache) with long writes, and to check the drive's TRIM and firmware state.

**What it means for the platform:** anything write-heavy will be bound by this disk: the distributed object store (D5), container image builds and database-like workloads. A storage benchmark in D5 has to be compared with these numbers, not with the drive's rating.

### Windows files seen from Linux (`/mnt/c`)

| Test | Linux ext4 | `/mnt/c` (9p file sharing) |
|---|---|---|
| Sequential read, 1 MiB | 1052 MB/s | 117 MB/s |
| Random read, 4 KiB | 25 500 IOPS (queue depth 32) | 4 759 IOPS (one request at a time) |

The `/mnt/c` tests could not use the same I/O engine (9p doesn't support Linux asynchronous direct I/O), so this isn't a like-for-like comparison. But the order of magnitude is clear: the same SSD is about 9× slower when Linux reads it through the Windows file-sharing layer. That's why `scripts/check.sh` builds in `~/arena-build` rather than in the repository on `/mnt/c`.

### Network

| Test (iperf3, TCP, 10 s) | Throughput | Retransmits |
|---|---|---|
| Loopback inside WSL | 25.7 Gbit/s | 36 |
| Container → host over Docker's default bridge | 18.1 Gbit/s (70%) | 491 |
| Wi-Fi link (from Windows) | 360 Mbit/s link speed | not measured |

- Loopback is the ceiling of the Linux network stack on this CPU.
- Docker's bridge adds a virtual Ethernet pair and network address translation, costing about 30% and many more retransmits. The Kubernetes cluster in D2 (k3d) runs its nodes as containers on this kind of network.
- A real wire would be the Wi-Fi link, about 70× slower than either. Network numbers measured inside one laptop are not what a real cluster would see.

### GPU

The MX350 is visible to Linux through WSL's GPU driver (`nvidia-smi` works). It is useful for small PyTorch work in the final-year project, but at 2 GB it is too small for GPU cluster experiments, as planned.

## What this means for the platform phases

| Resource | Budget |
|---|---|
| CPU | 4 vCPUs that scale almost linearly. Fine for a 3-node k3d cluster and a Slurm controller plus 2 workers, as long as nodes are containers, not VMs. |
| Memory | 3.8 GiB inside WSL, with about 3.3 GiB available when idle. The 2 VirtualBox VMs in D1 run outside WSL and come out of the laptop's remaining ~4 GB, so at most about 1.5 GB each, as planned. |
| Disk | Plenty of space on D:, but writes are slow (about 70 MB/s). Keep images small and expect write-heavy tests to be disk-bound. |
| Network | Container networking is far faster than any real link, so add delay and loss on purpose (`tc netem`, D6) to see realistic behaviour. |
