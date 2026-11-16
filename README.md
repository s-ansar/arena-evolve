# Arena Evolve

A networked, real-time robot-evolution simulator in C++. Sumo-style robots fight in a 2D arena. Neural-network controllers are evolved with a genetic algorithm, a multi-threaded server runs the simulation at a fixed tick rate, controllers connect over TCP/UDP, and a Qt app shows the arena live.

Inspired by the Arduino sumo robots built with the University of Birmingham Robotics & Drones Team.

## Status

| | Feature | Status |
|---|---|---|
| Simulation | 2D vectors, robots, fixed-timestep loop (headless and real-time) | Done |
| Simulation | Arena, collisions, win conditions | Done |
| Networking | TCP server, binary protocol, controllers as clients | Done |
| Networking | UDP state broadcast, latency measurements | Planned |
| Concurrency | Thread per client, simulation thread, thread-safe queues | Done |
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
- Raw POSIX TCP sockets (Linux), `std::thread`, mutexes and condition variables

Later: Google Benchmark, Qt 6, Docker, VirtualBox and Vagrant, k3d (Kubernetes), Prometheus and Grafana, Slurm, an S3-compatible object store.

## Project structure

```
arena-evolve/
├── src/sim/              # Simulation core: Vec2, Robot, Arena, game loops
├── src/net/              # Protocol, queues, sockets, server, client
├── src/ai/               # Controllers (baseline chase bot; evolved ones later)
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

## Run a match

On Linux, after building, start the server, then two controller clients:

```bash
./build/arena_server 5555
./build/arena_bot 127.0.0.1 5555             # chases the other robot
./build/arena_bot 127.0.0.1 5555 --passive   # sits still
```

## Design decisions

### Why raw POSIX sockets (Linux-only server)

The server uses `socket`, `bind`, `listen`, `accept`, `recv` and `send` directly instead of Boost.Asio.

- **Visibility.** Every network step is in the code: byte order, partial sends, `TCP_NODELAY`, `SIGPIPE`, `SO_REUSEADDR`, shutting a socket down to wake a blocked thread. A library would hide exactly the parts this project exists to show.
- **Simple threading.** One reader and one writer thread per client, with blocking calls, is easy to follow. Asio's asynchronous callback style is more scalable but harder to reason about.
- **The cost is portability.** The server and client build only on Linux. That matches where they run: WSL2, Docker and the Linux lab. The simulation core, protocol, queues and controllers have no socket code, so they still build and are tested on Windows in CI.
- **When this would change.** Thousands of clients would need non-blocking I/O with `epoll` (or Asio) instead of two threads per client. A match has two clients.

### Server threads

| Thread | Does | Talks to others through |
|---|---|---|
| Simulation (one) | Real-time tick loop; the only thread that touches the arena | Pops commands from a `ThreadSafeQueue`; puts states into each client's `Mailbox` |
| Reader (one per client) | Turns incoming bytes into commands | Pushes into the shared `ThreadSafeQueue` |
| Writer (one per client) | Sends the latest state | Takes from its `Mailbox`, which keeps only the newest state, so a slow client never delays the tick loop |

## Results

Measured numbers go here as the project grows: tick rate, missed deadlines, thread speed-up, evolved vs baseline win rate.
