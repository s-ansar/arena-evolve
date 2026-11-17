# Skills tracker

Tick a box only when the skill is genuinely used in this repo and you can explain it in an interview. Only ticked items may go on the CV. Fill in "Where" with a file or commit.

| Done | Area | Skill or tool | Where it appears |
|---|---|---|---|
| [x] | Language and build | C++20 | `src/`, `CMakeLists.txt` (designated initializers, `std::bit_cast`, `std::span`, defaulted `==`) |
| [x] | Language and build | CMake | `CMakeLists.txt`: static libraries, Linux-only targets, sanitizer option |
| [x] | Language and build | Ninja | `scripts/check.sh` (`-G Ninja`) |
| [x] | Language and build | vcpkg or FetchContent for dependencies | `CMakeLists.txt`: GoogleTest via FetchContent |
| [x] | Testing | GoogleTest | `tests/` (42 tests) |
| [ ] | Testing | GoogleMock | |
| [x] | Testing | CTest | `gtest_discover_tests`, `ctest` in CI and `check.sh` |
| [x] | Testing | Google Benchmark | `bench/sim_bench.cpp`; results in README |
| [ ] | Testing | Code coverage (gcov/lcov) | |
| [ ] | Debugging and quality | gdb | |
| [x] | Debugging and quality | AddressSanitizer | `.github/workflows/ci.yml`, `check.sh address` |
| [x] | Debugging and quality | ThreadSanitizer | `ci.yml` (mmap_rnd_bits fix); `tests/concurrency_test.cpp`, `tests/server_test.cpp` |
| [x] | Debugging and quality | UndefinedBehaviorSanitizer | `ci.yml`, `check.sh undefined` |
| [ ] | Debugging and quality | Valgrind | |
| [ ] | Debugging and quality | clang-tidy | |
| [ ] | Debugging and quality | clang-format | |
| [x] | Concurrency | std::thread | `src/net/server.cpp` (reader and writer thread per client) |
| [x] | Concurrency | Mutexes and condition variables | `src/net/thread_safe_queue.hpp`, `src/net/mailbox.hpp` |
| [x] | Concurrency | std::atomic | `src/net/metrics.hpp` (sim thread writes, HTTP thread reads; relaxed ordering explained) |
| [x] | Concurrency | Thread pool | `src/ai/thread_pool.cpp` (`std::latch`, reuses `ThreadSafeQueue`); speed-up in README |
| [x] | Concurrency | Thread-safe or lock-free queue | `src/net/thread_safe_queue.hpp` (mutex-based) |
| [x] | Networking | TCP sockets | `src/net/socket.cpp` (raw POSIX, TCP_NODELAY, MSG_NOSIGNAL, SO_SNDTIMEO) |
| [x] | Networking | UDP sockets | `src/net/socket.cpp` (sendto/recvfrom), `src/net/server.cpp` watcher broadcast |
| [x] | Networking | Non-blocking I/O | `MSG_DONTWAIT` UDP send/receive in the tick loop (`src/net/socket.cpp`) |
| [x] | Networking | Custom binary protocol | `src/net/protocol.cpp` (length-prefixed big-endian frames) |
| [x] | Networking | Wireshark capture and analysis | `docs/captures/match.pcap`, README "The protocol on the wire" |
| [ ] | Networking | Protocol Buffers (optional) | |
| [x] | Real-time | Fixed-timestep loop with std::chrono | `src/sim/loop.cpp` (`steady_clock`, `sleep_until`, drift-free deadlines) |
| [x] | Real-time | Deadline-miss and jitter statistics | `src/sim/timing_stats.hpp`, `src/sim/loop.cpp`; README "Results" |
| [x] | Real-time | Profiling with perf | README "Profiling with perf" (record, annotate, stat) |
| [x] | Computational intelligence | Neural network | `src/ai/network.cpp` (8-8-2 tanh MLP, forward pass from a flat genome) |
| [x] | Computational intelligence | Genetic algorithm (neuroevolution) | `src/ai/genetic.cpp` (tournament, uniform crossover, Gaussian mutation, elitism) |
| [x] | Computational intelligence | Fitness function and baseline comparison | `src/ai/match.cpp`; README "Neuroevolution vs the chase bot" (held-out angles, 5 seeds) |
| [x] | GUI | Qt Widgets | `src/gui/` (`QMainWindow`, `QTabWidget`, custom `QWidget::paintEvent` with `QPainter`) |
| [x] | GUI | Signals and slots | `src/gui/udp_watcher.cpp` (`readyRead`, `QTimer::timeout`, custom `stateReceived` signal) |
| [ ] | GUI | Model/view | |
| [x] | GUI | Qt Charts | `src/gui/main_window.cpp` (`QChart`, `QLineSeries`, `QValueAxis`) |
| [ ] | Engineering practice | Git branches and pull requests | |
| [x] | Engineering practice | GitHub Actions CI (Linux and Windows) | `.github/workflows/ci.yml` (5 jobs green) |
| [x] | Engineering practice | Docker | `Dockerfile` (multi-stage, three targets, non-root user) |
| [ ] | Engineering practice | Doxygen documentation | |
| [ ] | Engineering practice | Design patterns (observer, state, factory) | |
| [ ] | Engineering practice | GitHub Projects board (Agile) | |
| [ ] | Hardware (optional) | Arduino or ESP32 over serial | |
| [x] | Linux | Daily development on Ubuntu (WSL2) | all builds, tests and measurements run in WSL2 Ubuntu 26.04 |
| [x] | Linux | Bash scripting | `scripts/check.sh`, `infra/d0/*.sh` |
| [ ] | Linux | File permissions and processes | |
| [ ] | Linux | ssh with key-only login | |
| [ ] | Linux | systemd service | |
| [ ] | Linux | journalctl logs | |
| [ ] | Linux | ufw firewall | |
| [ ] | Linux | ss, ip and tcpdump | |
| [x] | Operations | Docker Compose | `compose.yaml` (server + two bots); results in README |
| [ ] | Operations | Prometheus metrics | |
| [ ] | Operations | Grafana dashboard | |
| [ ] | Operations | Deployment on an Ubuntu host (EC2 or Raspberry Pi) | |
| [ ] | Testing | Fuzzing with libFuzzer | |
| [ ] | Optional | TLS (OpenSSL) | |
| [ ] | VMs | VirtualBox virtual machines | |
| [ ] | VMs | Vagrant (reproducible VMs) | |
| [ ] | VMs | Snapshots and NAT vs host-only networking | |
| [ ] | Kubernetes | k3d or k3s cluster with kubectl | |
| [ ] | Kubernetes | Deployments, Services and ConfigMaps | |
| [ ] | Kubernetes | Resource requests/limits and probes | |
| [ ] | Kubernetes | Rolling update | |
| [ ] | Kubernetes | Jobs | |
| [ ] | Telemetry | node_exporter and kube-state-metrics | |
| [x] | Telemetry | Custom /metrics endpoint | `src/net/metrics.cpp`, `src/net/http_server.cpp` (Prometheus text format, /health) |
| [ ] | Telemetry | Alertmanager rules | |
| [ ] | Telemetry | SLO on a measured service level | |
| [ ] | Telemetry | Loki logs (optional) | |
| [ ] | Telemetry | OpenTelemetry tracing (optional) | |
| [ ] | HPC | Slurm cluster (slurmctld, slurmd, munge) | |
| [ ] | HPC | sbatch, squeue, sinfo and job arrays | |
| [ ] | HPC | Scaling study (speed-up, efficiency, Amdahl) | |
| [ ] | HPC | Same workload on Slurm and Kubernetes Jobs | |
| [ ] | HPC | OpenMP or MPI (optional) | |
| [ ] | HPC | University BlueBEAR cluster (if access allowed) | |
| [ ] | Storage | S3-compatible distributed object store | |
| [ ] | Storage | Erasure coding and node-failure test | |
| [x] | Storage | fio disk benchmarks | `infra/d0/baseline.sh`; slow-write investigation in `docs/hardware_map.md` |
| [ ] | Storage | Ceph/CRUSH concepts (reading only) | |
| [x] | Hardware | Hardware map (lscpu, lsblk, free, numactl, lspci) | `infra/d0/inventory.sh`, `docs/hardware_map.md` |
| [x] | Hardware | sysbench and stress-ng | `infra/d0/baseline.sh`; CPU and memory results in `docs/hardware_map.md` |
| [x] | Hardware | iperf3 network benchmark | loopback vs Docker bridge in `docs/hardware_map.md` |
| [ ] | Hardware | Raspberry Pi cluster nodes (optional) | |
| [ ] | Reliability | tc netem fault injection | |
| [ ] | Reliability | Game-day drills | |
| [ ] | Reliability | Runbooks | |
| [ ] | Reliability | Blameless post-incident reviews | |
| [ ] | Performance | Hypothesis, measurement and tuning report | |
| [ ] | Infrastructure as code | Makefile-driven lab (make lab-up) | |
| [ ] | Infrastructure as code | Ansible (optional) | |
