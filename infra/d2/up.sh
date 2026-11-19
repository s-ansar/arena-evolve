#!/usr/bin/env bash
# Phase D2: build the images, push them to a local registry, create a k3d cluster
# (1 server + 2 agent nodes, each a Docker container) that pulls from that registry, and
# deploy Arena Evolve.
#   bash infra/d2/up.sh          (about 3 minutes the first time)
#   k3d cluster delete arena     to remove the cluster (the registry stays)
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"

# A local container registry, the way a real cluster gets its images. (`k3d image import`,
# which copies images straight into each node, raced and then hung on this machine.)
# Docker on the host pushes to localhost:5050; the nodes pull from k3d-arena-registry:5000.
if ! k3d registry list arena-registry > /dev/null 2>&1; then
  k3d registry create arena-registry --port 5050
fi
push=localhost:5050/arena-evolve

echo "== images (version 1 and 2 of the server, for the rolling update test)"
docker build -q --target server --build-arg ARENA_VERSION=1 -t "$push/server:1" .
docker build -q --target server --build-arg ARENA_VERSION=2 -t "$push/server:2" .
docker build -q --target bot --build-arg ARENA_VERSION=1 -t "$push/bot:1" .
docker build -q --target fitness --build-arg ARENA_VERSION=1 -t "$push/fitness:1" .
for image in server:1 server:2 bot:1 fitness:1; do
  docker push -q "$push/$image"
done

echo "== cluster"
if ! k3d cluster list arena > /dev/null 2>&1; then
  # Traefik (k3s's default web router) is not needed and costs memory.
  k3d cluster create arena --servers 1 --agents 2 --registry-use k3d-arena-registry:5050 \
      --k3s-arg "--disable=traefik@server:0" --wait
fi
kubectl get nodes -o wide

echo "== deploy"
kubectl create namespace arena --dry-run=client -o yaml | kubectl apply -f -
# The genome files become a ConfigMap; the bot mounts it as /genomes.
kubectl -n arena create configmap arena-genomes --from-file=docs/genomes/ --dry-run=client -o yaml \
    | kubectl apply -f -
kubectl apply -f infra/d2/k8s/server.yaml -f infra/d2/k8s/bots.yaml
kubectl -n arena rollout status deployment/arena-server --timeout=180s
kubectl -n arena rollout status deployment/bot-evolved --timeout=180s
kubectl -n arena rollout status deployment/bot-chase --timeout=180s
kubectl -n arena get pods -o wide
