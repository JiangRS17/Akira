# extension-overhead

Measures Fabric domain-granularity scaling cost with **MPK isolation** enabled
(one fabric = one MPK key via `fabric_bridge_set_isolation(..., FABRIC_ISO_MPK, ...)`).

FlexOS layout stays fixed at one app compartment (`comp1`); Fabric domains are
built at runtime as a star:

| Bench | Domains | Components / domain | Target (last) |
|-------|---------|---------------------|---------------|
| `G0`  | 1       | 8                   | domain 0      |
| `G1`  | 2       | 4                   | domain 1      |
| `G2`  | 4       | 2                   | domain 3      |
| `G3`  | 8       | 1                   | domain 7      |

Each call is `fabric_gate(source, example8_empty)` so DFS walks more sibling
domains as the partition grows, while the callee stays the same empty function.

```sh
# single variant
make -j$(nproc) BENCH=G1 && ./run.sh

# full scalability sweep (G0..G3)
./run_all.sh
```

Requires QEMU `-cpu host` so the guest sees PKU (`./run.sh` already does this).