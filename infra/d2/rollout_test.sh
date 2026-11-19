#!/usr/bin/env bash
# Phase D2: does a rolling update of the server drop games?
#   bash infra/d2/rollout_test.sh [rollouts per setting]   (default 3)
#
# The bots play continuously and log every match as "finished" or "DROPPED" (connection
# lost mid-match). The test switches the server image between versions 1 and 2 several
# times and counts both kinds of line, first with the real 90 s grace period (the server
# drains: finishes its match, then exits), then as a control with a 1 s grace period
# (Kubernetes kills the server before the match can end).
set -euo pipefail
rounds="${1:-3}"
ns=arena

count() {  # count <pattern>: matching lines in both bots' logs since the test started
  local total=0 n
  for bot in bot-evolved bot-chase; do
    n=$(kubectl -n "$ns" logs "deploy/$bot" --since-time="$since" 2>/dev/null | grep -c "$1" || true)
    total=$((total + n))
  done
  echo "$total"
}

run_setting() {  # run_setting <grace seconds>
  local grace="$1" image version=2
  kubectl -n "$ns" patch deployment arena-server --type=merge \
      -p "{\"spec\":{\"template\":{\"spec\":{\"terminationGracePeriodSeconds\":$grace}}}}" > /dev/null
  kubectl -n "$ns" rollout status deployment/arena-server --timeout=180s > /dev/null
  sleep 10  # let matches settle after the patch's own rollout
  since=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  for _ in $(seq "$rounds"); do
    image="k3d-arena-registry:5050/arena-evolve/server:$version"
    kubectl -n "$ns" set image deployment/arena-server server="$image" > /dev/null
    kubectl -n "$ns" rollout status deployment/arena-server --timeout=180s > /dev/null
    sleep 8  # play a few matches on the new version before the next update
    version=$((3 - version))  # 2 -> 1 -> 2 ...
  done
  sleep 10
  # Each match is logged by both bots, so divide by two.
  echo "grace ${grace}s, $rounds rollouts: matches finished $(( $(count finished) / 2 )), dropped $(( $(count DROPPED) / 2 ))"
}

run_setting 90
run_setting 1
# Leave the deployment in its normal configuration.
kubectl -n "$ns" patch deployment arena-server --type=merge \
    -p '{"spec":{"template":{"spec":{"terminationGracePeriodSeconds":90}}}}' > /dev/null
kubectl -n "$ns" rollout status deployment/arena-server --timeout=180s > /dev/null
echo "server log of the last drain:"
kubectl -n "$ns" logs deploy/arena-server --tail=3 2>/dev/null || true
