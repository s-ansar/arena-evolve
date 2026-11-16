# Arena Evolve: handoff (2026-11-16, end of week 2)

Read `CLAUDE.md`, `SKILLS.md` and `docs/plan.md` first. Rules there still apply: keep code simple, explain each step briefly, owner must be able to explain every line, never claim a skill not backed by real code.

Open Claude Code in `C:\Users\hp\Documents\arena-evolve` (not `Documents\evolve`, which is an empty folder).

## State
- **Repo:** https://github.com/s-ansar/arena-evolve (public). `main` tracks `origin/main`. Repo-local git identity: "Salman Ansar" / `353674219+s-ansar@users.noreply.github.com`.
- **CI:** first run green on all 5 jobs (Ubuntu, Windows/MSVC, address, undefined, thread sanitizers).
- **Week 1 done:** `src/sim/`: `Arena`, robots with capped thrust and drag, equal-mass elastic collisions, ring-out, `winner`/`match_over`, `run_headless` and `run_realtime` (`steady_clock`, drift-free deadlines, late-tick count, `TickHook` after each tick).
- **Week 2 done:** `src/net/`: length-prefixed big-endian binary protocol (Welcome, Command, State), `ThreadSafeQueue`, latest-value `Mailbox`, RAII POSIX `Socket` (TCP_NODELAY, MSG_NOSIGNAL, SO_REUSEADDR, 1 s SO_SNDTIMEO), `Server` (sim thread + reader and writer thread per client), `play_match` client. `src/ai/chase.cpp` baseline bot. Programs `arena_server` and `arena_bot`.
- **Decision:** raw POSIX sockets, server Linux-only; reasons in README "Design decisions". Socket code is excluded on Windows in `CMakeLists.txt`.
- **Tests:** 42 (`sim_tests`, `net_core_tests`, `server_tests` Linux-only), including an end-to-end match over TCP and multi-threaded queue tests, so TSan now means something.
- **Earlier review findings:** all 5 resolved.
- **SKILLS.md:** 16 boxes ticked, each with where it appears. clang-format is configured but has not been run on the code yet, so it is not ticked.

## Environment
- WSL2 Ubuntu 26.04 (disk `D:\wsl\Ubuntu`), capped at 4 GB / 4 CPUs by `C:\Users\hp\.wslconfig`. systemd enabled, default user `hp`.
- Installed: g++ 15.2, cmake 4.2.3, ninja, gdb, clang 21, clang-format, clang-tidy.
- Build: `wsl -d Ubuntu -- bash scripts/check.sh [address|undefined|thread]` (builds in `~/arena-build[-san]` in Linux, not in the repo).
- Manual match: `~/arena-build/arena_server 5555`, then `arena_bot 127.0.0.1 5555` and `arena_bot 127.0.0.1 5555 --passive`.
- Installs: `sudo` with password did not work for the user; use `wsl -d Ubuntu -u root -- ...`. PowerShell mangles `$var` in inline bash, so use the Bash tool or a script file.
- `gh` is logged in as `s-ansar`. Watch CI with `gh run watch`.

## Known limitations (week 2)
- A client that sends a bad frame is ignored from then on; its robot keeps its last thrust.
- Commands take effect one tick after they arrive.
- The server accepts clients one by one with no timeout and plays one match at a time.

## Next steps
1. Week 3: UDP state broadcast, tick-rate and jitter statistics (extend `RunStats`), Google Benchmark for `sim::step`, a `perf` profile, a Wireshark capture of the protocol. Measured numbers go in the README "Results" section.
2. Run clang-format over the code (one commit), then tick it.
3. Week 4: neural-network controller and genetic algorithm; `run_headless` is ready for fitness evaluation; `ai::chase` is the baseline.

## Ideas discussed, not yet agreed
Skills not on the CV that would fit naturally: libFuzzer on `net::try_decode` (week 2 follow-up), an ESP32/FreeRTOS controller client, MPI for fitness evaluation in phase D4. The owner said the current scope is good; ask before adding any to `docs/plan.md`.

## Rules for the next session
Do not install, commit or push without the user's explicit OK.
