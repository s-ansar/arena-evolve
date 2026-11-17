#!/usr/bin/env bash
# Phase D1: set up the VM host inside WSL2. Run as root.
#   sudo bash infra/d1/install_host.sh
# Installs KVM/QEMU and libvirt (nested virtualization: WSL2 exposes /dev/kvm on
# Windows 11), Vagrant from HashiCorp's apt repository, and the vagrant-libvirt plugin.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
user="${SUDO_USER:-hp}"

apt-get update -q
apt-get install -y -q qemu-system-x86 qemu-utils libvirt-daemon-system libvirt-clients \
    libvirt-dev dnsmasq-base ebtables gcc make ruby-dev pkg-config nfs-kernel-server rsync curl gpg

# HashiCorp's repository (Vagrant left Ubuntu's own archive after the licence change).
install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://apt.releases.hashicorp.com/gpg | gpg --dearmor --yes -o /etc/apt/keyrings/hashicorp.gpg
. /etc/os-release
echo "deb [signed-by=/etc/apt/keyrings/hashicorp.gpg] https://apt.releases.hashicorp.com $VERSION_CODENAME main" \
    > /etc/apt/sources.list.d/hashicorp.list
apt-get update -q
apt-get install -y -q vagrant

systemctl enable --now libvirtd
usermod -aG libvirt,kvm "$user"

# The plugin is installed for the normal user, who runs vagrant.
sudo -u "$user" -H vagrant plugin install vagrant-libvirt

vagrant --version
sudo -u "$user" -H vagrant plugin list
virsh --connect qemu:///system version
