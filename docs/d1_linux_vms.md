# D1: Linux and VMs

Two Ubuntu 24.04 virtual machines, built from nothing by one command, with the Arena Evolve server running as a hardened systemd service.

```bash
sudo bash infra/d1/install_host.sh   # once: KVM/QEMU, libvirt, Vagrant + vagrant-libvirt
make vms-up                          # build both VMs from scratch (~5 minutes)
make vms-verify                      # 11 checks, PASS/FAIL each
bash infra/d1/snapshot_demo.sh       # save, break, restore
make vms-down                        # shut the VMs down (keeps them)
```

## Design

| | `arena-server` (192.168.56.10) | `arena-client` (192.168.56.11) |
|---|---|---|
| Role | Runs `arena_server` as a systemd service | Runs bots and watchers against the server |
| Size | 2 vCPUs, 1 GiB RAM, 8.7 GB disk | same |
| Provisioning | [`common.sh`](../infra/d1/provision/common.sh) then [`server.sh`](../infra/d1/provision/server.sh) | `common.sh` |

**Why KVM/libvirt instead of VirtualBox.** The plan named VirtualBox and Vagrant on Windows. Windows 11 already runs WSL2 under Hyper-V, and WSL2 exposes `/dev/kvm` (nested virtualization), so the VMs run on KVM, the Linux kernel's own hypervisor, managed by libvirt with the `vagrant-libvirt` provider. That keeps the whole lab in Linux and scriptable. It avoids the Windows installer and the VirtualBox-on-Hyper-V slowdown, and it is what Linux servers in data centres use. The Vagrant workflow (`up`, `ssh`, `provision`, `snapshot`, `destroy`) is the same.

**Networks** (as the server sees them):

| Interface | Network | What it is for |
|---|---|---|
| `ens5` 192.168.121.179/24 | libvirt NAT network (`vagrant-libvirt`) | Default route. The VM can reach the internet (apt) through the host's address translation, but nothing outside can open a connection to it. Vagrant's ssh uses it. |
| `ens6` 192.168.56.10/24 | Host-only network (no route out) | Only the host (192.168.56.1) and the other VM can reach it. Game, watcher and metrics traffic runs here. |

**Hardening.**

- **ssh** (drop-in `/etc/ssh/sshd_config.d/10-arena-hardening.conf`): keys only (`PasswordAuthentication no`, `KbdInteractiveAuthentication no`), `PermitRootLogin no`, `MaxAuthTries 3`, no X11 or agent forwarding. The file is named `10-` because sshd keeps the *first* value it reads, and the cloud image's own `60-cloudimg-settings.conf` would otherwise win. `sshd -t` checks the configuration before reloading, so a typo can't lock everyone out.
- **ufw**: deny all incoming by default. ssh is open. Ports 5555/tcp, 5555/udp and 8080/tcp are open only from 192.168.56.0/24.
- **systemd sandbox** ([`arena-server.service`](../infra/d1/arena-server.service)):
  - Runs as the system user `arena`, which has no shell and no home directory.
  - No Linux capabilities, `ProtectSystem=strict`, private `/tmp` and `/dev`.
  - Only IPv4 and Unix sockets allowed, and a system-call allow-list (`@system-service`, minus privileged calls).
  - `MemoryMax=256M`.
  - `systemd-analyze security` rates its exposure **1.3 ("OK")**, on a scale where 10 is unprotected.
- `Restart=on-failure` brings the server back after a crash. Logs go to the journal (`journalctl -u arena-server`).

## Results

`make vms-verify` after a fresh `vagrant destroy` and `vagrant up` (the fresh build took **281 s** for both VMs, compiling the programs inside each):

| Check | Result |
|---|---|
| `arena-server.service` active, running as user `arena` | PASS |
| A full match from the client VM to the server VM over the host-only network | PASS |
| The match result appears in `journalctl -u arena-server` | PASS |
| `/health` and `/metrics` reachable from the host, with the match counted | PASS |
| A listening but unlisted port (7000) works on the server itself but is blocked from the client | PASS |
| `ssh` with a password: `Permission denied (publickey)` before any password prompt | PASS |
| `sshd -T` reports `permitrootlogin no` | PASS |
| `kill -9` on the server: systemd starts a new process (new PID) within 4 s | PASS |

Snapshot test ([`snapshot_demo.sh`](../infra/d1/snapshot_demo.sh)): save snapshot `provisioned`, delete `/opt/arena/bin/arena_server` and stop the service (status `inactive`), restore. The restore took **13 s**, and the binary was back with the service `active`.

## What went wrong, and the fixes

1. **WSL stopped the VMs.** The VMs vanished seconds after `vagrant up` finished. The journal showed a full systemd shutdown of the Ubuntu instance: WSL stops a distribution about 15 s after the last Windows-side `wsl.exe` session ends, taking every service and VM with it. Fixed in `%UserProfile%\.wslconfig` with `instanceIdleTimeout=-1` and `vmIdleTimeout=-1`. A test service then survived 40 s with no session attached.
2. **Silent data loss from the unclean stop.** After that hard stop, the client VM's freshly installed genome file and binaries were **0 bytes**. ext4 had not yet written them out when QEMU was killed. Worse, Bash runs an empty executable as an empty script, so `arena_bot` "succeeded" while doing nothing. Re-provisioning didn't help at first, because Ninja saw the half-written outputs as up to date. Fixes: provisioning always builds in a fresh directory and ends with `sync`. The lesson for the game days (D6): check *what* a process did, not just its exit code.
