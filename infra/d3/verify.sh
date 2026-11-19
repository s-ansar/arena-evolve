#!/usr/bin/env bash
# Phase D3 checks: targets are scraped, the SLO rule computes, and alerts really fire.
#   bash infra/d3/verify.sh            (about 8 minutes: it waits for alerts to fire)
set -uo pipefail
ns=monitoring

kubectl -n $ns port-forward svc/prometheus 9090 > /dev/null 2>&1 &
pf1=$!
kubectl -n $ns port-forward svc/alertmanager 9093 > /dev/null 2>&1 &
pf2=$!
trap 'kill $pf1 $pf2 2> /dev/null' EXIT
sleep 3

query() {  # query <PromQL>: print "labels value" for each result
  curl -s --get http://localhost:9090/api/v1/query --data-urlencode "query=$1" | python3 -c '
import json, sys
for r in json.load(sys.stdin)["data"]["result"]:
    labels = ",".join(k + "=" + v for k, v in r["metric"].items() if k != "__name__")
    print("  " + (labels or "value") + " = " + r["value"][1])'
}
firing() {  # firing: alert names Alertmanager currently has
  curl -s http://localhost:9093/api/v2/alerts | python3 -c '
import json, sys
names = sorted({a["labels"]["alertname"] for a in json.load(sys.stdin) if a["status"]["state"] == "active"})
print("  firing:", ", ".join(names) if names else "(none)")'
}
wait_for_alert() {  # wait_for_alert <name> <max seconds>
  local start=$SECONDS
  while (( SECONDS - start < $2 )); do
    if curl -s http://localhost:9093/api/v2/alerts | grep -q "\"alertname\":\"$1\""; then
      echo "  $1 reached Alertmanager after $(( SECONDS - start )) s"; return 0
    fi
    sleep 5
  done
  echo "  $1 did NOT fire within $2 s"; return 1
}

echo "== scrape targets (1 = up)"
query 'up'
echo "== SLI: share of ticks on time (5m)"
query 'arena:tick_on_time:ratio_rate5m'
echo "== matches per minute"
query 'arena:matches:rate5m * 60'
echo "== alerts now"
firing

echo
echo "== experiment 1: stop the server (scale to 0)"
kubectl -n arena scale deployment/arena-server --replicas=0 > /dev/null
wait_for_alert ArenaServerDown 120
firing
kubectl -n arena scale deployment/arena-server --replicas=1 > /dev/null
kubectl -n arena rollout status deployment/arena-server --timeout=120s > /dev/null

echo
echo "== waiting for the SLO alert to be clear before experiment 2 (it looks back 5 minutes)"
start=$SECONDS
while curl -s http://localhost:9093/api/v2/alerts | grep -q '"alertname":"ArenaTickDeadlineSLOBreached"'; do
  (( SECONDS - start > 900 )) && { echo "  still firing after 900 s"; break; }
  sleep 15
done
echo "  clear after $(( SECONDS - start )) s"

echo
echo "== experiment 2: starve the server of CPU (limit 5 millicores)"
kubectl -n arena set resources deployment/arena-server -c server --requests=cpu=5m --limits=cpu=5m > /dev/null
kubectl -n arena rollout status deployment/arena-server --timeout=120s > /dev/null
wait_for_alert ArenaTickDeadlineSLOBreached 420
query 'arena:tick_on_time:ratio_rate5m'
query 'arena_last_match_lateness_max_seconds'
firing
echo "== restore the normal limit"
kubectl -n arena set resources deployment/arena-server -c server --requests=cpu=100m --limits=cpu=1 > /dev/null
kubectl -n arena rollout status deployment/arena-server --timeout=120s > /dev/null
