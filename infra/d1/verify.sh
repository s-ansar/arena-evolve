#!/usr/bin/env bash
# Phase D1 checks, run from WSL after `bash infra/d1/up.sh`. Each check prints PASS or FAIL.
#   bash infra/d1/verify.sh
set -uo pipefail
cd "$HOME/arena-d1/repo/infra/d1"
export VAGRANT_DEFAULT_PROVIDER=libvirt
server_ip=192.168.56.10
failures=0

# Start the VMs if they are stopped (after a WSL or laptop restart). Already-provisioned
# VMs just boot; nothing is rebuilt.
vagrant up --no-provision > /dev/null 2>&1

check() {  # check <description> <command...>: PASS if the command succeeds
  local what="$1"; shift
  if "$@" > /tmp/d1_check.out 2>&1; then
    echo "PASS  $what"
  else
    echo "FAIL  $what"; sed 's/^/      /' /tmp/d1_check.out | tail -5
    failures=$((failures + 1))
  fi
}
on_server() { vagrant ssh server -c "$1" -- -q; }
on_client() { vagrant ssh client -c "$1" -- -q; }

check "server: arena-server.service is active" on_server "systemctl is-active arena-server"
check "server: service runs as the unprivileged 'arena' user" \
  on_server "test \"\$(ps -o user= -C arena_server)\" = arena"

# A whole match between the VMs, over the private network.
check "client -> server: a full match over the private network" on_client \
  "arena_bot $server_ip 5555 --genome /opt/arena/genomes/seed1.txt > /tmp/evolved.log & sleep 0.3; arena_bot $server_ip 5555 > /dev/null; wait; grep -q 'match finished' /tmp/evolved.log"
check "server: the match result is in the journal" \
  on_server "sudo journalctl -u arena-server --since '-2 min' --no-pager | grep -q 'match over'"

# Monitoring from the host (192.168.56.1 is inside the allowed lab network).
check "host -> server: /health answers" curl -sf --max-time 3 "http://$server_ip:8080/health"
check "host -> server: /metrics counts the match" \
  bash -c "curl -sf --max-time 3 http://$server_ip:8080/metrics | grep -E '^arena_matches_total\{result=\"win\"\} [1-9]'"

# Firewall: a port that is listening but not allowed must be unreachable from outside.
on_server "sudo systemd-run --unit d1-probe --collect nc -lk 7000" > /dev/null 2>&1
check "firewall: port 7000 reachable on the server itself" on_server "nc -z -w 2 127.0.0.1 7000"
check "firewall: port 7000 blocked from the client" bash -c "! vagrant ssh client -c 'nc -z -w 3 $server_ip 7000' -- -q"
on_server "sudo systemctl stop d1-probe" > /dev/null 2>&1

# ssh: password logins must be refused before a password is even asked for.
check "ssh: password authentication is refused" bash -c \
  "ssh -o BatchMode=yes -o PreferredAuthentications=password -o PubkeyAuthentication=no \
       -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 \
       vagrant@$server_ip true 2>&1 | grep -q 'Permission denied (publickey)'"
check "ssh: root login is disabled" on_server "sudo sshd -T | grep -qx 'permitrootlogin no'"

# systemd restarts the server after a crash (Restart=on-failure).
check "systemd: restarts arena_server after it is killed" on_server \
  "old=\$(systemctl show -p MainPID --value arena-server); sudo kill -9 \$old; sleep 4; new=\$(systemctl show -p MainPID --value arena-server); test \"\$new\" != 0 && test \"\$new\" != \"\$old\" && systemctl is-active arena-server"

echo
on_server "sudo systemd-analyze security arena-server --no-pager | tail -1"
echo "failures: $failures"
exit "$failures"
