# Arena Evolve: handoff (2026-11-16)

Read `CLAUDE.md`, `SKILLS.md` and `docs/plan.md` first. Rules there still apply: keep code simple, explain each step briefly, owner must be able to explain every line, never claim a skill not backed by real code.

## State
- **Code:** `Vec2`, `Robot` (Euler `integrate`), 5 GoogleTest tests, CMake + FetchContent (googletest 1.15.2), CI workflow (Linux + Windows, plus address/undefined/thread sanitizer jobs). No arena code yet.
- **Commit:** `c8bea04` on `main` (11 files). Repo-local git identity: "Salman Ansar" / `353674219+s-ansar@users.noreply.github.com`. **No remote, nothing pushed.**
- **Milestone 1 done:** builds first time; 5/5 tests pass in Debug and under address, undefined and thread sanitizers.

## Environment
- WSL2 Ubuntu 26.04 (disk `D:\wsl\Ubuntu`), capped at 4 GB / 4 CPUs by `C:\Users\hp\.wslconfig`. systemd enabled, default user `hp`.
- Installed: g++ 15.2, cmake 4.2.3, ninja, gdb, clang 21, clang-format, clang-tidy.
- Build: `wsl -d Ubuntu -- bash scripts/check.sh [address|undefined|thread]` (builds in `~/arena-build[-san]` in Linux, not in the repo).
- Installs: `sudo` with password did not work for the user; use `wsl -d Ubuntu -u root -- ...`. PowerShell mangles `$var` in inline bash, so put multi-line shell in a script file.

## Uncommitted (created last, `check.sh` not yet run)
`scripts/check.sh`, `.gitattributes` (`* text=auto`, `*.sh text eol=lf`), `.claude/settings.json` (allows `check.sh` and read-only git), `.claude/skills/check/SKILL.md`, `docs/handoff.md`. Run `check.sh` and `check.sh thread` to confirm, then commit.

## Review findings still open
1. CI thread-sanitizer job may crash with "unexpected memory mapping" (ASLR). Fix: `sudo sysctl vm.mmap_rnd_bits=28` step.
2. `CMakeLists.txt`: add `DOWNLOAD_EXTRACT_TIMESTAMP TRUE` (CMP0135 warning).
3. `integrate(Robot&, double dt)` breaks the "no bare seconds" rule; take a `std::chrono` duration.
4. Week 2: POSIX sockets would break Windows CI; choose Boost.Asio or Linux-only server, and record why in the README.
5. Tests are single-threaded, so the TSan result proves nothing yet.

## Next steps
1. Run and commit the uncommitted files.
2. User creates empty GitHub repo `s-ansar/arena-evolve`; ask before adding the remote and pushing. Watch CI.
3. Fix review items 1-3, one commit each.
4. Tick only truly used boxes in `SKILLS.md` (C++20, CMake, GoogleTest, CTest, FetchContent, clang-format, sanitizers once CI is green).
5. Week 1 (about 7 Dec): `Arena`, collisions, win condition, fixed-timestep loop with `std::chrono`, tests first.

## Rules for the next session
Do not install, commit or push without the user's explicit OK.
