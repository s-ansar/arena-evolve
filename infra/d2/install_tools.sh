#!/usr/bin/env bash
# Phase D2: install kubectl and k3d (Kubernetes in Docker). Run as root.
#   sudo bash infra/d2/install_tools.sh
set -euo pipefail

# kubectl from the Kubernetes project's apt repository (pkgs.k8s.io).
k8s_minor=v1.34
install -m 0755 -d /etc/apt/keyrings
curl -fsSL "https://pkgs.k8s.io/core:/stable:/$k8s_minor/deb/Release.key" \
    | gpg --dearmor --yes -o /etc/apt/keyrings/kubernetes-apt-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/kubernetes-apt-keyring.gpg] https://pkgs.k8s.io/core:/stable:/$k8s_minor/deb/ /" \
    > /etc/apt/sources.list.d/kubernetes.list
apt-get update -q
DEBIAN_FRONTEND=noninteractive apt-get install -y -q kubectl

# k3d: a single static binary from its GitHub release, pinned to one version.
k3d_version=v5.8.3
curl -fsSL -o /usr/local/bin/k3d "https://github.com/k3d-io/k3d/releases/download/$k3d_version/k3d-linux-amd64"
chmod 0755 /usr/local/bin/k3d

kubectl version --client
k3d version
