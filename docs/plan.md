# Master plan v2: projects, infrastructure lab and skills

Updated 2026-11-16. One plan across four projects and four side tracks. It closes the gaps in Salman's CV: the C++/real-time/networking job posting, and the infrastructure/HPC posting (Linux, hardware, VMs, Kubernetes, distributed storage, telemetry, schedulers, CI/CD).

Rule for everything below: a skill goes on the CV only after it is ticked in `SKILLS.md` (really used, and explainable in an interview). Assumes about 8-10 focused hours a week alongside the final-year project.

## 0. The machine and what it means

| Fact (checked 2026-11-16) | Design decision |
|---|---|
| 7.8 GB RAM, 4 cores / 8 threads | Cluster "nodes" are **containers** (k3d, Docker Compose). At most **2 real VMs** at about 1.5 GB each. Cap WSL2 memory at 4 GB in `.wslconfig`. |
| C: has 37 GB free, D: has 310 GB free | Move the WSL2 distro and all VM and Docker data to **D:** (`wsl --export` / `--import`). |
| Windows 11 Home (no Hyper-V Manager), virtualization enabled | Use **WSL2** for Linux and **VirtualBox + Vagrant** for the real VMs. Run Docker Engine inside WSL2, not Docker Desktop (less RAM). |
| NVIDIA MX350 (2 GB), Intel Iris Xe | Useful for small PyTorch fine-tuning in the final-year project (check CUDA works). Too small for GPU cluster work. |
| No spare hardware | Optional: 2-3 Raspberry Pi nodes for a physical cluster (check current prices). Cloud only for short burst tests, with a budget alert set first. |

## 1. The four projects

| Project | Status | Size | What it proves |
|---|---|---|---|
| **A. Repped** | Built; finish the last features | ~1 week | Full-stack, AWS, Terraform, CI/CD, AI integration, security |
| **B. Final-year project** (solar-panel rover) | Assigned; preliminary report week of 7 Dec 2026 | Semester 1 software, Semester 2 hardware | Machine learning, computer vision, experiment design, ESP32 and sensor data |
| **C. Arena Evolve** (C++ application) | Skeleton only | 5 weeks | C++, TCP/IP, multithreading, real-time, computational intelligence, GUI |
| **D. Arena Platform** (infrastructure lab that runs Arena Evolve) | Not started | 6-7 weeks | Linux, VMs, Kubernetes, telemetry, Slurm/HPC, distributed storage, reliability, hardware and performance |

## 2. Coverage of both job postings

| Requirement | Where it is acquired | Evidence | P |
|---|---|---|---|
| Linux | Side track D, then D1 | Bash scripts, systemd unit, hardened Ubuntu VM | M |
| Python | Already strong; final-year project | Existing CV | done |
| Hardware infrastructure fundamentals | D0 hardware map and benchmarks | `docs/hardware_map.md` with measured vs expected numbers | M |
| Performance curiosity | C week 3, D7 performance report | Benchmarks, tuning experiments, graphs | M |
| Calm under pressure, prioritising | D6 game days | Runbooks and blameless post-incident reviews | M |
| Communication | D7 write-up and 5-minute demo; Robotics sprints | Written report, demo video | M |
| Containers | Already have (Docker); D2 | Dockerfiles, images | done |
| **Kubernetes** | D2 | k3d cluster, manifests | M |
| **VMs** | D1 | Vagrantfile, two Ubuntu VMs | M |
| **Distributed storage** | D5 | Multi-node S3-compatible store surviving a node loss | M |
| **Modern telemetry** | D3 | Prometheus, Grafana, alerts, logs, SLO | M |
| **HPC and schedulers** | D4 (and BlueBEAR if allowed) | Slurm job arrays, scaling study | M |
| CI/CD | Already have; extend to build and test the lab | GitHub Actions | done |
| C++, TCP/IP, threads, real-time, computational intelligence, GUI | Project C | The repo | M |
| Machine learning and computer vision | Project B | Classifier, metrics | M |
| Algorithms and SQL | Side track E | Online assessments | M |
| AWS Solutions Architect Associate | Side track F | Certificate | S |
| Agile and open source | Side track G | Board, one merged PR | S |

Priority: **M** must, **S** should, **C** could.

## 3. Project C: Arena Evolve application (5 weeks)

1. **Week 1:** simulation core (arena, collisions, fixed-timestep loop), tests, CI, sanitizers on Linux.
2. **Week 2:** TCP server, binary protocol, controller clients, thread per client plus simulation thread, thread-safe queues.
3. **Week 3:** real-time and measurement: UDP state broadcast, tick-rate and jitter statistics, Google Benchmark, `perf`, Wireshark.
4. **Week 4:** neuroevolution: neural-network controller, genetic algorithm, fitness evaluation in a local thread pool, baseline comparison. Keep fitness evaluation as a separate command-line program so the platform can run it anywhere.
5. **Week 5:** Qt live arena view and charts, Dockerfiles for the server and the fitness worker, a `/metrics` and `/health` endpoint on the server.

## 4. Project D: Arena Platform, phase by phase

Everything is code in `infra/` and starts with one command (`make lab-up`). Each phase ends with a short write-up in `docs/`.

| Phase | Build | Skills | Done when |
|---|---|---|---|
| **D0: hardware and baseline** (1 weekend) | Hardware map of the laptop (`lscpu`, `lsblk`, `free`, `numactl`, `lspci`, `nvidia-smi`). Baselines with `sysbench`, `stress-ng`, `fio`, `iperf3`. | Hardware fundamentals, benchmarking | `docs/hardware_map.md` shows spec vs measured CPU, memory, disk and network numbers |
| **D1: Linux and VMs** (1 week) | Vagrantfile for 2 Ubuntu VMs on VirtualBox. Bash provisioning. Arena server as a systemd service, key-only ssh, `ufw`, `journalctl`. Snapshots, NAT vs host-only networks. | Linux administration, VMs, infrastructure as code | Fresh `vagrant up` gives a hardened host running the server |
| **D2: containers and Kubernetes** (1 week) | k3d cluster (1 server + 2 agents). Deployments, Services, ConfigMaps, resource requests and limits, readiness and liveness probes, rolling update, Jobs. | Kubernetes, containers | Server runs in the cluster; a rolling update causes no dropped games |
| **D3: telemetry** (1 week) | `node_exporter`, kube-state-metrics, the server's `/metrics`. Prometheus, Grafana dashboards as code, Alertmanager. Logs with Loki (optional). An SLO: share of ticks that meet their deadline. | Modern telemetry | Dashboard shows tick latency and node health; an alert fires when the SLO is broken |
| **D4: schedulers and HPC** (1-2 weeks) | A Slurm cluster in containers (controller plus 2 workers, munge, `slurm.conf`). Run neuroevolution fitness as `sbatch --array` jobs. Run the same workload as Kubernetes Jobs. Scaling study: speed-up and efficiency vs workers, Amdahl's law. | HPC, Slurm, parallel scaling | Plot of speed-up with 1, 2, 4 workers; a comparison of Slurm vs Kubernetes scheduling |
| **D5: distributed storage** (1 week) | 4-node S3-compatible object store (MinIO; check its current licence, SeaweedFS is the fallback). Store genomes, replays and results through the S3 API. Stop a node and prove the data is still readable. Read about Ceph and CRUSH. | Distributed storage, erasure coding | Data survives a node loss; throughput measured with `fio` or a load tool |
| **D6: game days** (1 week) | Break things on purpose: kill a pod, kill a storage node, fill a disk, add latency with `tc netem`, starve CPU with `stress-ng`, drain a Slurm node. For each: a runbook and a short blameless post-incident review. | Incident response, reliability, writing under pressure | 4-6 runbooks and reviews |
| **D7: performance report and demo** (1 week) | Pick 2-3 hypotheses (for example: pinning threads to cores reduces jitter; container CPU limits change scaling). Measure, tune, graph, conclude. Record a 5-minute demo. | Scientific method, performance tuning, communication | `docs/performance_report.md` and a demo video |

Optional extras, only if time allows: Helm, Horizontal Pod Autoscaler, OpenTelemetry tracing, Ansible instead of shell provisioning, OpenMP or MPI in the fitness code, TLS on the protocol, a 2-3 Raspberry Pi cluster, GPU monitoring.

## 5. Side tracks

- **D. Linux (daily, 30 minutes).** WSL2 Ubuntu. Bash scripting, permissions, `grep`/`sed`/`awk`, processes and signals, ssh keys, cron and systemd, `ss`/`ip`/`tcpdump`, `apt`, Makefiles. Starts now.
- **E. Algorithms and SQL (5 problems a week).** NeetCode 150 pace plus HackerRank SQL. A timed online-assessment session every two weeks.
- **F. AWS Solutions Architect Associate.** Fixed study schedule, exam booked for February 2027, use Repped and the lab as labs.
- **G. Agile and open source.** Run the Robotics team in one-week sprints with a board and a short retrospective. One small merged pull request to an open-source project.

## 6. Timeline (adjust around your exam timetable)

| When | Focus |
|---|---|
| 16 Nov - 6 Dec | Final-year project first (preliminary report week of 7 Dec). Alongside: Linux daily, 5 algorithm problems a week, finish Repped, **D0** (one weekend). Install WSL2 and move it to D:. |
| 7 - 20 Dec | Project C weeks 1-2 |
| 21 Dec - 10 Jan | Project C weeks 3-4 |
| 11 - 31 Jan | Project C week 5, **D1** and **D2** |
| Feb | **D3**, **D4**, **D5**; AWS exam |
| Mar | **D6**, **D7**; open-source PR; if allowed, run the final-year project training or the Slurm study on BlueBEAR (ask Dr Donaldson) |
| Mar - Apr | Polish READMEs with real numbers, update the CVs (add an Infrastructure version), final-year project report |

## 7. If time runs short

- **Minimum viable lab (about 3 weekends):** D0, D1, a small D2, a basic D3, and one game day. That still covers Linux, VMs, Kubernetes, telemetry, hardware and reliability.
- **Cut in this order:** optional extras, D5 storage, D4 Kubernetes-Jobs comparison, Qt GUI polish. Never cut Linux, the real-time measurements, telemetry, or the final-year project.
- **Apply now** with the current CV. Many graduate schemes close in December and January, so don't wait for the projects. Mention a project only when there is real code to open.

## 8. Rules

- Every piece of infrastructure is code in the repo; nothing is configured by hand without a script or a note.
- Measure before tuning. Every performance claim has a graph and the command that produced it.
- Every number on the CV comes from this repo's `docs/` write-ups.
- Be able to explain every file in an interview.
