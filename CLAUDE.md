# Arena Evolve — notes for Claude Code

Arena Evolve is a networked, real-time robot-evolution simulator written in C++. Sumo-style robots fight in a 2D arena. Their neural-network controllers are evolved with a genetic algorithm. A multi-threaded server runs the simulation at a fixed tick rate, controllers connect to it over TCP/UDP, and a Qt desktop app shows the arena live.

It also runs on a small infrastructure lab ("Arena Platform": Linux VMs, Kubernetes, Slurm, distributed storage, telemetry) so it can prove infrastructure and HPC skills as well. This is a portfolio project for graduate job applications, and a second project after Repped. It exists to prove skills that job postings ask for by name: TCP/IP network programming, multi-threaded applications, real-time programming, computational intelligence and GUI programming. **The owner must be able to explain every line in an interview.** So:

- Keep code simple and readable. Choose the boring option over the clever one.
- Explain non-obvious decisions in short comments, especially anything about threads, locks, timing or the network protocol.
- When you add a feature, briefly explain what you did and why at the end, including how it can fail and how it was tested.
- Make small, focused changes. Don't rewrite unrelated files.
- Never claim a skill that isn't really used. `SKILLS.md` is the source of truth for what may go on the CV.

## Stack

- C++20, CMake (3.20+), compiled with GCC, Clang or MSVC
- GoogleTest for tests, run through CTest. Google Benchmark from week 2
- Sockets: POSIX or Boost.Asio (decide in week 2, write down why in the README)
- Qt 6 for the GUI (week 5)
- GitHub Actions CI on Linux and Windows, plus Address, Undefined and Thread sanitizer jobs
- Platform lab (all in `infra/`, started with `make lab-up`): WSL2 Ubuntu, VirtualBox + Vagrant (2 VMs), Docker Engine, k3d (Kubernetes), Prometheus, Grafana, Alertmanager, Slurm in containers, an S3-compatible distributed object store
- Machine limits: 7.8 GB RAM, 4 cores. Cluster nodes are containers; at most 2 real VMs at about 1.5 GB each. Keep WSL2, VM and Docker data on D:, not C:

## Conventions

- Headers and sources live in `src/`, grouped by module (`src/sim`, `src/net`, `src/ai`, `src/gui`). Everything is in the `sim`, `net`, `ai` or `gui` namespace. No `using namespace std`.
- Resource ownership uses RAII and `std::unique_ptr`. No raw owning pointers, no manual `new` or `delete`.
- Every piece of shared state says in a comment which threads touch it and what protects it (mutex, atomic or none because it is single-threaded).
- Time uses `std::chrono` (`steady_clock` for loops, never `system_clock`). No bare numbers for seconds or milliseconds.
- Every module gets tests in `tests/`. Anything involving threads must pass ThreadSanitizer.
- Format with `clang-format` (see `.clang-format`).
- Infrastructure is code: scripts, Vagrantfiles, Compose files, Kubernetes manifests and config live in `infra/`. Nothing is configured by hand without a script or a note.
- Measure before tuning. Every performance claim has a graph and the command that produced it.
- Every phase of the lab ends with a short write-up in `docs/` (decisions, results, what broke).

## Before saying a task is done

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

If you changed threaded code, also build once with `-DSANITIZE=thread`. For memory code, use `-DSANITIZE=address`.

## Roadmap

The full plan, machine limits, side tracks and timeline are in `docs/plan.md`. Develop and test on Linux (Ubuntu under WSL2).

### Application (Project C, 5 weeks)

1. **Week 1: simulation core.** Arena, collisions, fixed-timestep loop, unit tests, CI, sanitizers. (Skeleton exists: `Vec2`, `Robot`, first tests.)
2. **Week 2: networking and threads.** TCP server, length-prefixed binary protocol, controllers as clients, thread per client plus the simulation thread, thread-safe queues.
3. **Week 3: real-time and measurement.** UDP state broadcast, tick-rate and jitter statistics, Google Benchmark, `perf`, Wireshark capture.
4. **Week 4: computational intelligence.** Neural-network controller, genetic algorithm, fitness in a local thread pool, baseline comparison. Fitness evaluation is a separate command-line program so the platform can run it anywhere.
5. **Week 5: GUI and containers.** Qt live arena view, Dockerfiles for the server and the fitness worker, `/metrics` and `/health` endpoints.

### Platform lab (Project D, about 6-7 weeks, starts with D0 now)

- **D0 hardware and baseline:** hardware map, sysbench, stress-ng, fio, iperf3.
- **D1 Linux and VMs:** Vagrant + VirtualBox, systemd service, key-only ssh, ufw, journalctl.
- **D2 containers and Kubernetes:** k3d cluster, Deployments, Services, probes, rolling update, Jobs.
- **D3 telemetry:** node_exporter, kube-state-metrics, app metrics, Prometheus, Grafana as code, Alertmanager, an SLO on tick deadlines.
- **D4 schedulers and HPC:** Slurm in containers, fitness as `sbatch --array`, the same work as Kubernetes Jobs, a scaling study.
- **D5 distributed storage:** 4-node S3-compatible store, node-failure test.
- **D6 game days:** deliberate failures, runbooks, blameless post-incident reviews.
- **D7 performance report and demo:** hypotheses, measurements, tuning, graphs, 5-minute demo.

Record real numbers in the README and `docs/` as they appear (tick rate, missed deadlines, speed-up, failover time, evolved vs baseline win rate). CV bullets only use numbers measured here.
