#!/usr/bin/env bash
# Phase D3: deploy the monitoring stack into the k3d cluster from D2.
#   bash infra/d3/up.sh
# Then, from WSL (or Windows, through WSL's localhost forwarding):
#   kubectl -n monitoring port-forward svc/grafana 3000 &       http://localhost:3000
#   kubectl -n monitoring port-forward svc/prometheus 9090 &    http://localhost:9090
#   kubectl -n monitoring port-forward svc/alertmanager 9093 &  http://localhost:9093
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"

kubectl apply -f infra/d3/k8s/monitoring.yaml -f infra/d3/k8s/prometheus-config.yaml
# The dashboard JSON lives in its own file (easier to edit); it becomes a ConfigMap here.
kubectl -n monitoring create configmap grafana-dashboards --from-file=infra/d3/dashboards/ \
    --dry-run=client -o yaml | kubectl apply -f -
# Pick up config changes in running pods.
kubectl -n monitoring rollout restart deployment/prometheus deployment/grafana > /dev/null
for d in prometheus alertmanager grafana kube-state-metrics; do
  kubectl -n monitoring rollout status "deployment/$d" --timeout=300s
done
kubectl -n monitoring rollout status daemonset/node-exporter --timeout=300s
kubectl -n monitoring get pods -o wide
