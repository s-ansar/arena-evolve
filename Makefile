# Entry points for the Arena Platform lab. Each phase adds its targets here; the plan's
# end goal is a single `make lab-up`. Run from WSL (Linux), in the repository root.

.PHONY: help hardware

help:
	@echo "make hardware   D0: hardware inventory, baseline benchmarks and a summary (~4 min)"

# D0: record the hardware Linux sees, benchmark CPU, memory, disk and network, and print
# the headline numbers. Raw results go to docs/hardware/raw/.
hardware:
	bash infra/d0/inventory.sh
	bash infra/d0/baseline.sh
	bash infra/d0/summarise.sh
