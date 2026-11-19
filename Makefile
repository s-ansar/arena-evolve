# Entry points for the Arena Platform lab. Run from WSL (Linux), in the repository root.
# `make lab-up` brings up the Kubernetes lab with monitoring; the other targets run single phases.

.PHONY: help hardware vms-up vms-verify vms-down vms-destroy k8s-up k8s-rollout-test k8s-down \
        monitoring-up lab-up

help:
	@echo "make lab-up            Kubernetes cluster + Arena Evolve + monitoring (D2 + D3)"
	@echo "make hardware          D0: hardware inventory, baseline benchmarks and a summary (~4 min)"
	@echo "make vms-up            D1: build/boot the two KVM VMs with Vagrant (~5 min the first time)"
	@echo "make vms-verify        D1: run the 11 PASS/FAIL checks against the VMs"
	@echo "make vms-down          D1: shut the VMs down (they keep their disks)"
	@echo "make vms-destroy       D1: delete the VMs completely"
	@echo "make k8s-up            D2: registry, images, k3d cluster, deploy the game (~3 min)"
	@echo "make k8s-rollout-test  D2: rolling updates with and without graceful drain"
	@echo "make k8s-down          D2: delete the k3d cluster"
	@echo "make monitoring-up     D3: Prometheus, Alertmanager, Grafana, exporters"

# D0: record the hardware Linux sees, benchmark CPU, memory, disk and network, and print
# the headline numbers. Raw results go to docs/hardware/raw/.
hardware:
	bash infra/d0/inventory.sh
	bash infra/d0/baseline.sh
	bash infra/d0/summarise.sh

# D1: Vagrant + libvirt/KVM (host setup once: sudo bash infra/d1/install_host.sh).
vms-up:
	bash infra/d1/up.sh
vms-verify:
	bash infra/d1/verify.sh
vms-down:
	bash infra/d1/up.sh halt
vms-destroy:
	bash infra/d1/up.sh destroy -f

# D2: k3d (tools once: sudo bash infra/d2/install_tools.sh).
k8s-up:
	bash infra/d2/up.sh
k8s-rollout-test:
	bash infra/d2/rollout_test.sh 10
k8s-down:
	k3d cluster delete arena

# D3: monitoring stack inside the D2 cluster.
monitoring-up:
	bash infra/d3/up.sh

lab-up: k8s-up monitoring-up
