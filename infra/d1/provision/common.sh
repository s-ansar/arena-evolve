#!/usr/bin/env bash
# Runs as root on every VM at first `vagrant up` (and on `vagrant provision`).
# Safe to run again: every step either checks first or simply overwrites.
#   1. build the Arena Evolve programs from /vagrant into /opt/arena/bin
#   2. harden ssh: keys only, no root login
#   3. firewall: deny everything incoming except ssh (server.sh opens the game ports)
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

echo "== packages"
apt-get update -q
apt-get install -y -q --no-install-recommends g++ cmake ninja-build ufw curl netcat-openbsd

echo "== build"
# Build outside the synced folder, without tests or benchmarks (they would download
# GoogleTest and Google Benchmark). Two parallel jobs to stay inside 1 GiB of RAM.
# Always from a clean directory: after an unclean VM stop, half-written outputs can look
# "up to date" to Ninja (this happened: 0-byte binaries that silently did nothing).
rm -rf /var/tmp/arena-build
cmake -S /vagrant -B /var/tmp/arena-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DARENA_BUILD_TESTS=OFF -DARENA_BUILD_BENCHMARKS=OFF > /dev/null
cmake --build /var/tmp/arena-build -j 2 --target arena_server arena_bot arena_watch arena_fitness
install -d /opt/arena/bin /opt/arena/genomes
install -m 0755 /var/tmp/arena-build/{arena_server,arena_bot,arena_watch,arena_fitness} /opt/arena/bin/
install -m 0644 /vagrant/docs/genomes/*.txt /opt/arena/genomes/
ln -sf /opt/arena/bin/* /usr/local/bin/

echo "== ssh hardening"
# sshd uses the FIRST value it reads for each setting, and reads sshd_config.d/*.conf in
# name order, so "10-" wins over the cloud image's own "60-cloudimg-settings.conf".
cat > /etc/ssh/sshd_config.d/10-arena-hardening.conf <<'EOF'
# Arena Evolve lab hardening (infra/d1/provision/common.sh)
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin no
PubkeyAuthentication yes
MaxAuthTries 3
X11Forwarding no
AllowAgentForwarding no
EOF
sshd -t                      # refuse to continue with a broken config (would lock us out)
systemctl reload ssh

echo "== firewall"
ufw default deny incoming
ufw default allow outgoing
ufw allow OpenSSH            # Vagrant's own ssh, over the NAT network
ufw --force enable
ufw status verbose

# Flush everything to the virtual disk now. The first run of this lab lost a freshly
# installed file (0 bytes after an unclean VM stop: ext4 had not written it out yet).
sync
