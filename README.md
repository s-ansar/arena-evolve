# Arena Evolve

A networked, real-time robot-evolution simulator in C++. Sumo-style robots fight in a 2D arena. Neural-network controllers are evolved with a genetic algorithm, a multi-threaded server runs the simulation at a fixed tick rate, controllers connect over TCP/UDP, and a Qt app shows the arena live.

Inspired by the Arduino sumo robots built with the University of Birmingham Robotics & Drones Team.

## Status

| | Feature | Status |
|---|---|---|
| Simulation | 2D vectors, robots, fixed-timestep integration | In progress |
| Simulation | Arena, collisions, win conditions | Planned |
| Networking | TCP server, binary protocol, controllers as clients | Planned |
| Networking | UDP state broadcast, latency measurements | Planned |
| Concurrency | Thread per client, simulation thread, thread-safe queues | Planned |
| Real-time | Fixed tick rate with deadline-miss and jitter statistics | Planned |
| AI | Neural-network controller evolved by a genetic algorithm | Planned |
| AI | Parallel fitness evaluation, baseline comparison | Planned |
| GUI | Qt live arena view and fitness charts | Planned |
| Platform | Hardware map and benchmarks (CPU, memory, disk, network) | Planned |
| Platform | Ubuntu VMs with Vagrant, systemd service, hardened ssh | Planned |
| Platform | Kubernetes (k3d) cluster running the server | Planned |
| Platform | Prometheus, Grafana and alerts for tick latency and node health | Planned |
| Platform | Slurm cluster running neuroevolution as job arrays, scaling study | Planned |
| Platform | Distributed object storage with a node-failure test | Planned |
| Platform | Game days, runbooks and a performance report | Planned |

## Tech stack

- C++20, CMake
- GoogleTest and CTest
- GitHub Actions CI on Linux and Windows, plus Address, Undefined and Thread sanitizer jobs

Later: sockets (POSIX or Boost.Asio), Google Benchmark, Qt 6, Docker, VirtualBox and Vagrant, k3d (Kubernetes), Prometheus and Grafana, Slurm, an S3-compatible object store.

## Project structure

```
arena-evolve/
├── src/sim/              # Simulation core: Vec2, Robot (arena and loop come next)
├── tests/                # GoogleTest unit tests
├── .github/workflows/    # CI: build and test, plus sanitizers
├── infra/                # Platform lab as code (VMs, Kubernetes, Slurm, storage, monitoring), coming
├── docs/                 # plan.md, hardware map, runbooks, performance report
├── CMakeLists.txt
├── CLAUDE.md             # Working rules and roadmap
└── SKILLS.md             # Tracker: which skills are really used
```

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers (GCC or Clang on Linux):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=thread   # or address, undefined
```

## Results

Measured numbers go here as the project grows: tick rate, missed deadlines, thread speed-up, evolved vs baseline win rate.
