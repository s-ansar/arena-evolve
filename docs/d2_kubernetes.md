# D2: containers and Kubernetes

Arena Evolve on a 3-node Kubernetes cluster (k3d: k3s nodes running as Docker containers inside WSL2), with a rolling update that does not drop games.

```bash
sudo bash infra/d2/install_tools.sh   # once: kubectl 1.34, k3d 5.8.3
make k8s-up                           # registry, images, cluster, deploy (~3 minutes)
make k8s-rollout-test                 # 10 rolling updates with and without graceful drain
kubectl apply -f infra/d2/k8s/evolve-job.yaml   # batch Job: evolution with 4 seeds
make k8s-down                         # delete the cluster
```

## What runs where

| Object | File | Purpose |
|---|---|---|
| Namespace `arena` | `up.sh` | Keeps the game separate from `monitoring` (D3) |
| ConfigMap `arena-genomes` | built from `docs/genomes/` | The evolved genome, mounted into the bot at `/genomes`, so a new genome needs no new image |
| Deployment + Service `arena-server` | [`server.yaml`](../infra/d2/k8s/server.yaml) | 1 replica (a match needs both players on one server). Service ports 5555/tcp, 5555/udp, 8080/tcp |
| Deployments `bot-evolved`, `bot-chase` | [`bots.yaml`](../infra/d2/k8s/bots.yaml) | Play match after match through the Service (`--forever`) |
| Indexed Job `evolve` | [`evolve-job.yaml`](../infra/d2/k8s/evolve-job.yaml) | 4 evolution runs (seed = completion index + 1), 2 pods at a time |

Every pod:
- runs as non-root UID 10001 with a read-only root file system and all Linux capabilities dropped, under the runtime's default seccomp profile;
- has CPU and memory requests (what the scheduler reserves) and limits (CPU is throttled at the limit, and memory over the limit gets the container killed).

The server has two probes:

| Probe | Endpoint | What a failure does |
|---|---|---|
| Liveness | `/health` | Restarts the container |
| Readiness | `/ready` | Takes the pod out of the Service (new connections stop going there). Fails on purpose while the server is draining |

Images come from a local registry (`k3d-arena-registry`), the way a real cluster gets them. The first attempt copied images straight into the nodes with `k3d image import`. That raced between nodes ("failed to ingest … unexpected EOF") and, in direct mode, hung for over 14 minutes, so it was replaced.

## Rolling updates without dropped games

A rolling update replaces the server pod with a new version. With `maxSurge: 1, maxUnavailable: 0`, the new pod starts first. Once it is ready, the old pod gets `SIGTERM`, then `SIGKILL` if it hasn't exited when `terminationGracePeriodSeconds` runs out. Killing a server mid-match drops that game for both players.

The fix is in the server (C++, commit `782a89d`):

1. **Signals on a thread.** `SIGTERM` is blocked in every thread and handled by one thread waiting in `sigwait()`. The handling code is then ordinary code, not an async signal handler, where almost nothing is safe to call.
2. **Drain.** `request_stop()` sets a `draining` flag and shuts the listening socket. No new match can start. A match in progress doesn't use the listener, so it plays to the end, and then the process exits.
3. **Readiness.** `/ready` returns 503 while draining, so Kubernetes stops routing new players to the old pod.
4. **"Welcome" means the game is on.** Welcome is sent only once both players are connected. A client cut off before then knows no game was lost, and just reconnects.
5. **Grace period.** `terminationGracePeriodSeconds: 90` is longer than the longest match (60 s), so a drain always gets to finish.

**Measuring it.** Each bot logs every match as `finished` or `DROPPED` (connection lost before the final, decided state). [`rollout_test.sh`](../infra/d2/rollout_test.sh) switches the image between versions 1 and 2 ten times while the bots play, then counts. As a control, it repeats the same thing with a 1-second grace period, so the server is killed before it can drain:

| Setting | Rolling updates | Matches finished | Matches dropped |
|---|---|---|---|
| Graceful drain, 90 s grace | 10 | 30 | **0** |
| Control, 1 s grace | 10 | 32 | **2** |

Why the control drops only about 1 in 5 updates: the old pod gets `SIGTERM` about 1 s after the new pod starts. A 3.2-second match is cut off only if it started less than about 1.2 s before that, so roughly a third of updates are at risk, and a 3-update trial happened to drop none. That's why the final test uses 10. Deleting the pod by hand in the middle of a match with `--grace-period=1` drops the game every time ("match 55 DROPPED at tick 176" on both bots), which shows the detection itself works.

## Batch Job

The Indexed Job ran 4 evolution runs, 2 at a time, spread across the nodes, in **52 s** wall time. The results are identical to the native runs (seeds 1–4: 32, 32, 30 and 20 wins out of 32), because evolution is deterministic, so a container changes nothing about the result.

The training phase took 13.5 s per pod, about 4× slower than the same single-threaded run natively. That's under investigation in D4's scaling study (likely causes: the CPU limit's throttling, and sharing 4 vCPUs with the cluster and the bots).

## Resource use

`kubectl top nodes` while the Job ran: the 3 nodes used 1.0, 0.5 and 1.2 CPU cores, and 146, 181 and 453 MiB of memory (the server node also runs the Kubernetes control plane).
