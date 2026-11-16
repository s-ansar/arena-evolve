# Arena Evolve

A networked, real-time robot-evolution simulator in C++. Sumo-style robots fight in a 2D arena. Neural-network controllers are evolved with a genetic algorithm, a multi-threaded server runs the simulation at a fixed tick rate, controllers connect over TCP/UDP, and a Qt app shows the arena live.

Inspired by the Arduino sumo robots built with the University of Birmingham Robotics & Drones Team.

## Status

| | Feature | Status |
|---|---|---|
| Simulation | 2D vectors, robots, fixed-timestep loop (headless and real-time) | Done |
| Simulation | Arena, collisions, win conditions | Done |
| Networking | TCP server, binary protocol, controllers as clients | Done |
| Networking | UDP state broadcast, latency measurements | Done |
| Concurrency | Thread per client, simulation thread, thread-safe queues | Done |
| Real-time | Fixed tick rate with deadline-miss and jitter statistics | Done |
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
- Raw POSIX TCP and UDP sockets (Linux), `std::thread`, mutexes and condition variables
- Google Benchmark

Later: Qt 6, Docker, VirtualBox and Vagrant, k3d (Kubernetes), Prometheus and Grafana, Slurm, an S3-compatible object store.

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
./build/arena_watch 127.0.0.1 5555 10        # optional: watch over UDP for 10 s, print loss and latency
```

The server prints the winner and the tick timing after each match.

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

Measured on 2026-11-16 on the development laptop: 4 cores at 2.4 GHz, WSL2 Ubuntu 26.04, g++ 15.2, Release build. Single runs, so treat them as indicative, not as averages over many sessions.

### Real-time tick loop (one match, 60 ticks per second, 156 ticks)

| Measure | Value |
|---|---|
| Target tick interval | 16 667 µs |
| Mean interval | 16 665 µs |
| Jitter (standard deviation of the interval) | 102 µs (0.6% of a tick) |
| Shortest / longest interval | 16 355 / 16 990 µs |
| Wake-up lateness, mean / max | 301 / 617 µs |
| Ticks that started after their deadline | 0 |

The mean interval matches the target because each deadline is computed from the start time (`start + n * period`), so lateness never adds up into drift. Wake-up lateness of about 0.3 ms is the OS scheduler and timer slack in a WSL2 virtual machine. It is under 4% of the tick budget.

### UDP state broadcast (same match, `arena_watch` on the same machine)

| Measure | Value |
|---|---|
| State datagrams received | 156 of 156 |
| Lost / out of order | 0 / 0 |
| One-way latency, mean (stddev) | 268 µs (106 µs) |
| One-way latency, min / max | 120 / 807 µs |

Latency is measured with the system-wide monotonic clock on both ends, so it is only valid on one machine. Over localhost no packets were lost. Loss would show up on a real network or under `tc netem` (planned for the game days).

### Microbenchmarks (Google Benchmark, `sim_bench`, mean of 3 runs)

| Benchmark | Time per call |
|---|---|
| `sim::step`, 2 robots (a real match) | 10.4 ns |
| `sim::step`, 8 / 16 / 32 / 64 robots | 72 ns / 261 ns / 973 ns / 3.5 µs |
| Fitted complexity of `sim::step` | O(N²), RMS error 9% |
| Encode a 2-robot State | 138 ns |
| Decode a 2-robot State | 54 ns |

One physics step for a match takes about 10 ns of a 16.7 ms tick, so the server spends almost all of each tick asleep. The all-pairs collision check grows as N², as expected. It would only matter with hundreds of robots, and a spatial grid would fix it then. Encoding is slower than decoding because it builds the payload in a second vector and then copies it into the frame.
