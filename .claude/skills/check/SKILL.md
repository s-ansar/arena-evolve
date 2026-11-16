---
name: check
description: Build and test Arena Evolve in WSL2 Ubuntu, optionally under a sanitizer. Use before saying a task is done.
---

Run the project's build-and-test script. The argument is optional: `address`, `undefined` or `thread`.

- From PowerShell: `wsl -d Ubuntu -- bash scripts/check.sh [sanitizer]`
- From a Linux shell: `bash scripts/check.sh [sanitizer]`

Rules:
- Run with no argument first. If the task touched threaded code, also run with `thread`; for memory code, also run with `address`.
- Report the number of tests passed. If something fails, show the failing test output and do not claim the task is done.
- Do not edit the script to make a failure go away; fix the code or the test.
