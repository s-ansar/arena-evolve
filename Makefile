# Entry points for the Arena Platform lab. Each phase adds its targets here; the plan's
# end goal is a single `make lab-up`. Run from WSL (Linux), in the repository root.

.PHONY: help hardware vms-up vms-verify vms-down vms-destroy

help:
	@echo "make hardware     D0: hardware inventory, baseline benchmarks and a summary (~4 min)"
	@echo "make vms-up       D1: build/boot the two KVM VMs with Vagrant (~5 min the first time)"
	@echo "make vms-verify   D1: run the 11 PASS/FAIL checks against the VMs"
	@echo "make vms-down     D1: shut the VMs down (they keep their disks)"
	@echo "make vms-destroy  D1: delete the VMs completely"

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
